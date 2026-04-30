# 列车通信网络同牵引单元双网关冗余功能设计

## 1. 目标与约束

- 两个网关（GW-A/GW-B）同挂一条 MVB 网络，并具备以太网点对点链路。
- 任一时刻仅允许一个网关为**冗余主（MASTER）**参与业务转发；另一台为**冗余从（STANDBY）**热备。
- 冗余切换原则：
  - 从设备检测不到主设备的 MVB 与以太网心跳，或超过“收帧超时倍数 × 刷新周期”后，提升为主。
  - 发生主从切换后**不自动回切**（non-revertive）。
  - 原主恢复后若检测到总线上已有冗余主，则保持从设备。

## 2. 角色与状态机

### 2.1 状态定义

- `INIT`：上电初始化。
- `DISCOVERY`：MVB 端口发现 + 以太网邻居发现。
- `ELECTION`：主从协商。
- `MASTER`：主角色，参与业务转发并上送从状态。
- `STANDBY`：从角色，热备监控。
- `FAULT`：本机故障保护态（可选，按产品安全策略实现）。

### 2.2 状态迁移（核心）

1. 上电进入 `INIT`，本地硬件、协议栈自检通过后转 `DISCOVERY`。
2. `DISCOVERY`：
   - 通过 MVB 总线主调度完成端口发现，确认对端网关 MVB 端口存在。
   - 通过以太网点对点链路互发 `HELLO`，确认链路可达。
3. `ELECTION`：
   - 若总线上已存在有效主声明（`MASTER_ANNOUNCE`），本机进入 `STANDBY`。
   - 否则按固定优先级（如设备ID/配置优先级/MAC字典序）决主，胜者进入 `MASTER`，败者进入 `STANDBY`。
4. `MASTER` 运行期间周期发送双通道心跳（MVB + ETH）。
5. `STANDBY` 同时监测两路心跳：
   - 条件A：MVB心跳超时；
   - 条件B：ETH心跳超时；
   - 条件C：接收帧更新时间超过 `N × T_refresh`（N为超时倍数）。
   - 当 `(A && B) || C` 成立，切换为 `MASTER`。
6. 原 `MASTER` 恢复后重新进入 `DISCOVERY/ELECTION`，若检测到当前已有主，保持 `STANDBY`。

> 建议：`C` 用于兜底处理“链路假活着但业务停刷”的异常。

## 3. 冗余协商与报文建议

## 3.1 报文类型（逻辑）

- `HELLO`：上电邻居发现，携带设备标识、软件版本、能力位。
- `MASTER_ANNOUNCE`：主声明，携带主设备ID、任期号（epoch）、时间戳。
- `HEARTBEAT`：主/从周期心跳，携带角色、运行状态、计数器。
- `STATUS_SYNC`：从设备状态上报给主（温度、CPU、链路、故障码等）。
- `TAKEOVER_NOTICE`：从切主后广播接管通知。

## 3.2 关键字段

- `device_id`：全局唯一。
- `priority`：静态优先级（值越小优先级越高或反之，需统一）。
- `epoch`：主任期号，主切换时递增，防旧主抢占。
- `hb_seq`：心跳序号。
- `role`：MASTER/STANDBY。
- `health_bitmap`：本机健康位图。

## 4. 定时参数建议（可配置）

- `T_hb`：心跳周期（示例 100 ms）。
- `T_timeout_mvb`：MVB心跳超时（示例 3 × `T_hb`）。
- `T_timeout_eth`：ETH心跳超时（示例 3 × `T_hb`）。
- `T_refresh`：业务数据刷新周期（示例 50 ms）。
- `N_refresh_lost`：刷新超时倍数（示例 N=6）。
- `T_debounce`：切换去抖时间（示例 50~100 ms）。

触发建议：

- 升主判据：`(lost_mvb >= T_timeout_mvb && lost_eth >= T_timeout_eth) || lost_refresh >= N_refresh_lost * T_refresh`。
- 避免误切换：判据成立后再经过 `T_debounce` 二次确认。

## 5. 主从职责划分

### 5.1 MASTER

- 执行业务数据转发。
- 接收 `STANDBY` 的 `STATUS_SYNC` 并转发到 MVB 总线。
- 发布 `MASTER_ANNOUNCE` 与 `HEARTBEAT`。
- 维护 `epoch` 与主状态机日志。

### 5.2 STANDBY

- 不参与业务转发（或仅做镜像接收，不对外发布业务）。
- 周期发送本机状态给主。
- 双链路监视主心跳与刷新时效。
- 故障条件满足时执行接管，进入 `MASTER` 并广播 `TAKEOVER_NOTICE`。

## 6. 非回切（Non-Revertive）策略

- 一旦 `STANDBY` 成功接管为 `MASTER`，即便原主恢复，也不自动回切。
- 原主恢复后通过发现流程检测到现网已有主，应锁定为 `STANDBY`。
- 如需人工回切，仅允许在检修模式由维护命令触发。

## 7. 异常场景与保护

- **脑裂防护**：
  - 强制要求“主声明 + 任期号（epoch）”机制。
  - 同时收到两个主声明时，优先保留更高 `epoch`；若相同则按 `priority/device_id` 决议。
- **单通道失效**：
  - 仅 MVB 或仅 ETH 失效时不立即切换；双通道均失效或刷新超时才切换。
- **时钟漂移**：
  - 使用单调时钟计时，避免系统时间跳变导致误判。
- **重启风暴抑制**：
  - 增加最小主保持时间 `T_master_hold`（如 2 s），避免频繁抖动。

## 8. 伪代码示例

```text
on startup:
  state = DISCOVERY

loop every 10ms:
  update_link_status()
  update_rx_timestamps()

  switch state:
    DISCOVERY:
      if mvb_peer_found && eth_peer_found:
        state = ELECTION

    ELECTION:
      if detected_master_announce:
        state = STANDBY
      else if win_by_priority():
        epoch = epoch + 1
        become_master()
        state = MASTER
      else:
        state = STANDBY

    MASTER:
      forward_business_data()
      send_heartbeat(mvb, eth)
      relay_standby_status_to_mvb()
      if self_fault_critical:
        degrade_or_reset_by_policy()

    STANDBY:
      send_status_sync_to_master()
      if ((mvb_hb_timeout && eth_hb_timeout) || refresh_timeout_nx):
        wait(T_debounce)
        if confirm_condition_still_true:
          epoch = max(observed_epoch, epoch) + 1
          broadcast_takeover_notice()
          become_master()
          state = MASTER
```

## 9. 验证用例（最小集）

1. **正常上电**：A/B均上电，完成发现与选主，仅一个主对外转发。
2. **主断电**：主掉电，从在阈值内切主成功。
3. **主网口断链**：仅ETH断，仍可维持角色不误切换（若MVB与刷新正常）。
4. **主MVB异常**：仅MVB异常，仍不误切。
5. **双通道丢失**：从按阈值切主。
6. **原主恢复**：恢复后识别现有主并保持从，不回切。
7. **脑裂模拟**：注入双主声明，按epoch/priority收敛到单主。

## 10. 实施建议

- 先实现“状态机 + 双心跳 + 非回切”最小闭环。
- 再加入 epoch、防脑裂、去抖与诊断日志。
- 参数全部可配置并支持在线读取，便于车辆级联调。
