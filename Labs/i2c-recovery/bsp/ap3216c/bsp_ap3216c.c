#include "bsp_ap3216c.h"
#include <stdint.h>


#define AUTO_FAULT_ENABLE        1        /* 1=启用自动注入, 0=关闭 */
#define AUTO_FAULT_PERIOD_MS     5000     /* 两次注入之间的空档(上一次释放 -> 下一次开始) */
#define AUTO_FAULT_HOLD_MS       1000     /* 保持SDA拉低时长 */

/* ================= Event Queue =================
 * 目标: 统一日志输出, 避免在状态机里直接 printf 产生乱序/重复。
 * 仅实现自动注入相关事件, 后续可扩展手动/检测/恢复事件。
 */
typedef enum {
    AP_EVT_AUTO_START = 1,   /* 自动注入开始 */
    AP_EVT_AUTO_RELEASE      /* 自动注入释放 */
} ap_evt_type_t;

typedef struct {
    uint8_t   type;          /* ap_evt_type_t */
    uint8_t   _reserved;     /* 对齐占位 */
    uint16_t  hold_ms;       /* 仅在 RELEASE 时有效 */
    uint32_t  ts;            /* 事件时间戳 (ms) */
    uint32_t  seq;           /* auto 注入序号 */
    uint16_t  base_ir, base_ps, base_als; /* START 基线 */
    uint16_t  end_ir,  end_ps,  end_als;  /* RELEASE 结束 */
} ap_evt_t;

#define AP_EVT_QSIZE  16
static ap_evt_t ap_evt_q[AP_EVT_QSIZE];
static volatile unsigned ap_evt_head = 0; /* push 写入位置 */
static volatile unsigned ap_evt_tail = 0; /* pop 读取位置 */
static volatile unsigned ap_evt_drops = 0; /* 溢出丢弃计数 */

static void ap_evt_push(const ap_evt_t *e)
{
    unsigned next = (ap_evt_head + 1) % AP_EVT_QSIZE;
    if (next == ap_evt_tail) { /* full */
        ap_evt_drops++;
        return; /* 丢弃最新 */
    }
    ap_evt_q[ap_evt_head] = *e; /* 结构体拷贝 */
    ap_evt_head = next;
}

/* 对外给 main 调用: 输出并清空事件 (建议放在 while 循环中) */
void rec_evt_pump(void)
{
    while (ap_evt_tail != ap_evt_head) {
        ap_evt_t e = ap_evt_q[ap_evt_tail];
        ap_evt_tail = (ap_evt_tail + 1) % AP_EVT_QSIZE;
        switch (e.type) {
        case AP_EVT_AUTO_START:
         //统一 CSV 风格字段: version,mode,seq,phase,ts,hold_ms,base_x,end_x,delta_x,drops
         printf("af_csv version=1 mode=auto seq=%u phase=inject ts=%u hold_ms=0 base_ir=%u base_ps=%u base_als=%u end_ir=%u end_ps=%u end_als=%u delta_ir=0 delta_ps=0 delta_als=0 drops=%u\r\n",
             (unsigned)e.seq, (unsigned)e.ts,
             (unsigned)e.base_ir, (unsigned)e.base_ps, (unsigned)e.base_als,
             (unsigned)e.base_ir, (unsigned)e.base_ps, (unsigned)e.base_als,
             (unsigned)ap_evt_drops);
            break;
        case AP_EVT_AUTO_RELEASE: {
         int dir = (int)e.end_ir  - (int)e.base_ir;
         int dps = (int)e.end_ps  - (int)e.base_ps;
         int dals = (int)e.end_als - (int)e.base_als;
         printf("af_csv version=1 mode=auto seq=%u phase=release ts=%u hold_ms=%u base_ir=%u base_ps=%u base_als=%u end_ir=%u end_ps=%u end_als=%u delta_ir=%d delta_ps=%d delta_als=%d drops=%u\r\n",
             (unsigned)e.seq, (unsigned)e.ts, (unsigned)e.hold_ms,
             (unsigned)e.base_ir, (unsigned)e.base_ps, (unsigned)e.base_als,
             (unsigned)e.end_ir, (unsigned)e.end_ps, (unsigned)e.end_als,
             dir, dps, dals,
             (unsigned)ap_evt_drops);
            break; }
        default:
         printf("af_csv version=1 mode=auto seq=%u phase=unknown ts=%u hold_ms=%u base_ir=%u base_ps=%u base_als=%u end_ir=%u end_ps=%u end_als=%u delta_ir=0 delta_ps=0 delta_als=0 drops=%u type=%u\r\n",
             (unsigned)e.seq, (unsigned)e.ts, (unsigned)e.hold_ms,
             (unsigned)e.base_ir, (unsigned)e.base_ps, (unsigned)e.base_als,
             (unsigned)e.end_ir, (unsigned)e.end_ps, (unsigned)e.end_als,
             (unsigned)ap_evt_drops, (unsigned)e.type);
            break;
        }
    }
}

#if AUTO_FAULT_ENABLE
volatile int g_i2c_blocked;

typedef enum {
    AUTO_FAULT_IDLE = 0,
    AUTO_FAULT_INJECTING
} auto_fault_state_t;

static auto_fault_state_t af_state = AUTO_FAULT_IDLE;
static uint32_t af_next_due_ms = 0;        /* 下一次注入计划时间 */
static uint32_t af_start_ts = 0;           /* 当前注入开始时间 */
static uint32_t af_seq = 0;                /* 自动注入序号 */
static uint16_t af_base_ir=0, af_base_ps=0, af_base_als=0;
static uint16_t af_end_ir=0,  af_end_ps=0,  af_end_als=0;

/* 对外接口: 在 main 的 while(1) 中调用 */
void fault_auto_process(uint32_t time)
{
    uint32_t now = time;

    switch(af_state) {
    case AUTO_FAULT_IDLE:
        if (af_next_due_ms == 0) {
            af_next_due_ms = now + AUTO_FAULT_PERIOD_MS; /* 首次设定 */
        } else if ((int)(now - af_next_due_ms) >= 0) {
            /* 触发一次自动注入 */
            af_seq++;
            af_start_ts = now;
            /* 采 baseline 传感器(失败也不阻塞流程) */
            ap3216c_readdata(&af_base_ir, &af_base_ps, &af_base_als);
            p2_force_pull_sda();
            g_i2c_blocked = 1; /* 标记故障中 */
            /* 推送 START 事件 */
            ap_evt_t ev = {0};
            ev.type = AP_EVT_AUTO_START;
            ev.seq  = af_seq;
            ev.ts   = af_start_ts;
            ev.base_ir = af_base_ir; ev.base_ps = af_base_ps; ev.base_als = af_base_als;
            ap_evt_push(&ev);
            af_state = AUTO_FAULT_INJECTING;
        }
        break;
    case AUTO_FAULT_INJECTING:
        if ((now - af_start_ts) >= AUTO_FAULT_HOLD_MS) {
            /* 释放 */
            p2_backto_pad();
            ap3216c_readdata(&af_end_ir, &af_end_ps, &af_end_als); /* 结束采样 */
            uint32_t hold_ms = now - af_start_ts;
            g_i2c_blocked = 0;
            ap_evt_t ev = {0};
            ev.type = AP_EVT_AUTO_RELEASE;
            ev.seq  = af_seq;
            ev.ts   = now;
            ev.hold_ms = (uint16_t)hold_ms; /* 若>65535 自动截断, 足够用 */
            ev.base_ir = af_base_ir; ev.base_ps = af_base_ps; ev.base_als = af_base_als;
            ev.end_ir  = af_end_ir;  ev.end_ps  = af_end_ps;  ev.end_als  = af_end_als;
            ap_evt_push(&ev);
            af_state = AUTO_FAULT_IDLE;
            af_next_due_ms = now + AUTO_FAULT_PERIOD_MS; /* 安排下一次 */
        }
        break;
    default:
        af_state = AUTO_FAULT_IDLE;
        break;
    }
}
#endif /* AUTO_FAULT_ENABLE */

void p2_force_pull_sda(void)
{
    if(I2C1->I2SR & (1 << 5))
    {
        i2c_master_stop(I2C1);
    }
    
    i2c_disable(I2C1);

    IOMUXC_SetPinMux(IOMUXC_UART4_RX_DATA_GPIO1_IO29, 0); // SION=0
    IOMUXC_SetPinConfig(IOMUXC_UART4_RX_DATA_GPIO1_IO29, 0x10B0); // 常用 GPIO 安全 PAD 配置（示例值）

    //set GPIO1_IO29 -- low 
    _gpio_pin_config_t cfg;
    cfg.direction = kGPIO_DigitalOutput;
    cfg.outputLogic = 0; 
    cfg._gpio_interrupt_mode = kGPIO_NoIntmode;
    gpio_init(GPIO1, 29, &cfg);
    gpio_pinwrite(GPIO1, 29, 0); 
}

void p2_backto_pad(void)
{
    //set GPIO1_IO29 -- high
    GPIO1->GDIR &= ~(1<<29);
    IOMUXC_SetPinMux(IOMUXC_UART4_TX_DATA_I2C1_SCL, 1); /*复用为I2C1_SCL */
    IOMUXC_SetPinMux(IOMUXC_UART4_RX_DATA_I2C1_SDA, 1); /*复用为I2C1_SDA */
    IOMUXC_SetPinConfig(IOMUXC_UART4_TX_DATA_I2C1_SCL, 0x70B0);
    IOMUXC_SetPinConfig(IOMUXC_UART4_RX_DATA_I2C1_SDA, 0x70B0);
    i2c_init(I2C1);
}

unsigned char ap3216c_init(void)
{
    unsigned char value = 0;

    // Init IO
    IOMUXC_SetPinMux(IOMUXC_UART4_TX_DATA_I2C1_SCL, 1); /*复用为I2C1_SCL */
    IOMUXC_SetPinMux(IOMUXC_UART4_RX_DATA_I2C1_SDA , 1); /*复用为I2C1_SDA */

    IOMUXC_SetPinConfig(IOMUXC_UART4_TX_DATA_I2C1_SCL, 0X70b0);
    IOMUXC_SetPinConfig(IOMUXC_UART4_RX_DATA_I2C1_SDA, 0X70b0);

    // Init I2C
    i2c_init(I2C1);

    // Init ap3216
    ap3216c_writeonebyte(AP3216C_ADDR, AP3216C_SYSTEMCONG, 0X4); //reset
    delayms(50);
    ap3216c_writeonebyte(AP3216C_ADDR, AP3216C_SYSTEMCONG, 0X3); //reset
    value = ap3216c_readonebyte(AP3216C_ADDR, AP3216C_SYSTEMCONG);
    printf("ap3216c system config reg=%#x\r\n", value);

    if(value == 0x3)
        return 0;
    else
        return 1;
}

unsigned char ap3216c_readonebyte(unsigned char addr, unsigned char reg)
{
    unsigned char val  = 0;
    
    struct i2c_transfer masterXfer;

    masterXfer.slaveAddress = addr;
    masterXfer.direction = kI2C_Read;
    masterXfer.subaddress = reg;
    masterXfer.subaddressSize = 1;
    masterXfer.data = &val;
    masterXfer.dataSize = 1;
    i2c_master_transfer(I2C1, &masterXfer);
    return val;
}

unsigned char ap3216c_writeonebyte(unsigned char addr, unsigned char reg, 
                                    unsigned char data)
{
    unsigned char writedata  = data;
    unsigned char status = 0;
    struct i2c_transfer masterXfer;

    masterXfer.slaveAddress = addr;
    masterXfer.direction = kI2C_Write;
    masterXfer.subaddress = reg;
    masterXfer.subaddressSize = 1;
    masterXfer.data = &writedata;
    masterXfer.dataSize = 1;
    if(i2c_master_transfer(I2C1, &masterXfer))
        status = 1;
    return status;
}

void ap3216c_readdata(unsigned short *ir, unsigned short *ps, 
                    unsigned short *als)
{
    unsigned char buf[6];
    unsigned char i = 0;

    // circle read data
    for(i = 0; i < 6; i++) {
        buf[i] = ap3216c_readonebyte(AP3216C_ADDR, AP3216C_IRDATALOW + i);
    }

    if(buf[0] & 0x80){ // if true IR PS data is nouse
        *ir = 0;
        *ps = 0;
    } else     {
        *ir = ((unsigned short)buf[1] << 2) | (buf[0] & 0x03);
        *ps = (((unsigned short)buf[5] & 0x3F) << 4) | (buf[4] & 0x0F);
    }

    *als  = ((unsigned short)buf[3] << 8) | buf[2]; 
}