/**
 * data_logger.h — 车载数据记录器 (黑匣子)
 *
 * CSV 格式循环记录关键 CAN 信号到 SD 卡
 * 周期: 100ms, 保留最近 4-8 小时数据
 * 事件触发冻结: 故障时保存前后各 10 秒数据
 */

#ifndef DATA_LOGGER_H
#define DATA_LOGGER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化数据记录器 (打开/创建日志文件)
 * @return 0=成功
 */
int Logger_Init(void);

/**
 * @brief 写入一行 CSV 数据 (由主循环周期性调用)
 * @note  内部自动判断是否达到 100ms 周期
 */
void Logger_WriteRecord(void);

/**
 * @brief 冻结事件: 保存触发时刻前后各 10 秒数据
 * @param event_type 事件类型名称
 */
void Logger_FreezeEvent(const char *event_type);

/**
 * @brief 存储管理: 删除最旧文件确保空间
 */
void Logger_ManageStorage(void);

#ifdef __cplusplus
}
#endif

#endif /* DATA_LOGGER_H */
