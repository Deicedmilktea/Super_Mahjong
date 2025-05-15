// 由中断服务程序 (ISR) 更新的红外计数值
volatile uint32_t g_ir1_raw_count = 0;
volatile uint32_t g_ir2_raw_count = 0;

// 主循环中使用的计数值副本，防止直接在循环中多次读取 volatile 变量
uint32_t current_ir1_count = 0;
uint32_t current_ir2_count = 0;

// 期望的红外计数值 (根据上下文判断是到达还是被取走)
uint32_t expected_ir1_target_count = 0;
uint32_t expected_ir2_target_count = 0;

// 当前机器状态
typedef enum
{
    STATE_MACHINE_IDLE,

    // 发牌阶段 - 第1步：送两张牌 (底层)
    STATE_DEAL_P1_LOAD_BOTTOM_START,
    STATE_DEAL_P1_LOAD_BOTTOM_WAIT_ARRIVAL,
    STATE_DEAL_P1_LOWER_TRAY_START,
    STATE_DEAL_P1_LOWER_TRAY_WAIT_COMPLETION,

    // 发牌阶段 - 第2步：再送两张牌 (上层)
    STATE_DEAL_P1_LOAD_TOP_START,
    STATE_DEAL_P1_LOAD_TOP_WAIT_ARRIVAL,
    STATE_DEAL_P1_RAISE_TRAY_START,
    STATE_DEAL_P1_RAISE_TRAY_WAIT_COMPLETION,

    // 跳牌阶段
    STATE_JUMP_START,
    STATE_JUMP_LIFT_TWO_START, // 第一次升两张
    STATE_JUMP_LIFT_TWO_WAIT_COMPLETION,
    STATE_JUMP_AWAIT_TAKE_TWO,
    STATE_JUMP_LIFT_ONE_START, // 后续升一张
    STATE_JUMP_LIFT_ONE_WAIT_COMPLETION,
    STATE_JUMP_AWAIT_TAKE_ONE,
    STATE_JUMP_CHECK_NEXT, // 检查跳牌是否结束

    // 常规行牌阶段
    STATE_NORMAL_LIFT_ONE_START,
    STATE_NORMAL_LIFT_ONE_WAIT_COMPLETION,
    STATE_NORMAL_AWAIT_TAKE_ONE,
    STATE_NORMAL_LOWER_TRAY_START,
    STATE_NORMAL_LOWER_TRAY_WAIT_COMPLETION,
    STATE_NORMAL_RELOAD_ONE_START,
    STATE_NORMAL_RELOAD_ONE_WAIT_ARRIVAL,
    STATE_NORMAL_RAISE_TRAY_START,
    STATE_NORMAL_RAISE_TRAY_WAIT_COMPLETION,

    STATE_ERROR // 错误状态
} MachineState_TypeDef;

MachineState_TypeDef g_current_machine_state = STATE_MACHINE_IDLE;

// 辅助变量
uint8_t g_jump_phase_lift_count = 0;        // 跳牌阶段已升牌次数 (0:准备升两张, 1:已升两张, 2/3/4:已升后续的单张)
uint8_t g_tile_lifted_from_conveyor_id = 0; // 记录当前升起的牌来自哪个传送带 (1 或 2)
bool g_action_in_progress_flag = false;     // 标记一个需要等待完成的动作是否已启动

// 超时定时器相关
uint32_t g_state_timeout_start_time = 0;
const uint32_t TIMEOUT_DURATION_MS = 10000; // 例如10秒超时

// 获取当前毫秒数 (用于超时)
uint32_t Get_Current_Time_ms(void);

// 传送带控制
void Conveyor_Start(uint8_t conveyor_id); // id = 1 or 2
void Conveyor_Stop(uint8_t conveyor_id);
bool Is_Conveyor_Running(uint8_t conveyor_id); // 可选，用于更精细控制

// 托盘控制
void Tray_Move_Down(void);
void Tray_Move_To_Flat(void); // 抬至平面
bool Is_Tray_At_Bottom(void); // 通过限位开关或传感器检测
bool Is_Tray_At_Flat(void);   // 通过限位开关或传感器检测

// 升牌机构控制
void LiftMechanism_Lift_Two_Tiles_For_Jump(void); // 跳牌阶段，升起两张牌
bool Is_LiftMechanism_Lift_Two_Tiles_Done(void);

void LiftMechanism_Lift_One_Tile(uint8_t conveyor_id_source); // 从指定传送带牌堆升一张
bool Is_LiftMechanism_Lift_One_Tile_Done(void);

// 其他
void System_Error_Handler(const char *error_message);   // 错误处理函数
void Start_Button_Check_And_Clear_Press(bool *pressed); // 检查启动按钮是否按下

void main_control_loop(void)
{
    // 1. 读取并清零全局中断计数值 (推荐做法，避免在循环中多次访问volatile)
    //    实际操作中，如果ISR仅仅是递增，且主循环读取频率远高于ISR触发频率，
    //    直接读取g_irX_raw_count也可以，但复制出来更安全。
    //    为简化伪代码，我们假设在每个case的条件判断中直接使用g_irX_raw_count。
    //    current_ir1_count = g_ir1_raw_count; (假设在需要时读取)
    //    current_ir2_count = g_ir2_raw_count;

    // 2. 状态机处理
    switch (g_current_machine_state)
    {
    case STATE_MACHINE_IDLE:
        // 重置所有相关计数器和标志
        g_jump_phase_lift_count = 0;
        g_action_in_progress_flag = false;
        // Conveyor_Stop(1); // 确保电机停止
        // Conveyor_Stop(2);

        bool start_pressed = false;
        Start_Button_Check_And_Clear_Press(&start_pressed);
        if (start_pressed)
        {
            g_current_machine_state = STATE_DEAL_P1_LOAD_BOTTOM_START;
        }
        break;

    // ========= 发牌阶段 - 第1步：送两张牌 (底层) =========
    case STATE_DEAL_P1_LOAD_BOTTOM_START:
        expected_ir1_target_count = g_ir1_raw_count + 1;
        expected_ir2_target_count = g_ir2_raw_count + 1;
        Conveyor_Start(1);
        Conveyor_Start(2);
        g_state_timeout_start_time = Get_Current_Time_ms(); // 启动超时计时
        g_current_machine_state = STATE_DEAL_P1_LOAD_BOTTOM_WAIT_ARRIVAL;
        break;

    case STATE_DEAL_P1_LOAD_BOTTOM_WAIT_ARRIVAL:
        if (g_ir1_raw_count >= expected_ir1_target_count && g_ir2_raw_count >= expected_ir2_target_count)
        {
            Conveyor_Stop(1);
            Conveyor_Stop(2);
            g_current_machine_state = STATE_DEAL_P1_LOWER_TRAY_START;
        }
        else if (Get_Current_Time_ms() - g_state_timeout_start_time > TIMEOUT_DURATION_MS)
        {
            System_Error_Handler("Timeout: Deal P1 Load Bottom Arrival");
            g_current_machine_state = STATE_ERROR;
        }
        break;

    case STATE_DEAL_P1_LOWER_TRAY_START:
        if (!g_action_in_progress_flag)
        { // 避免重复启动动作
            Tray_Move_Down();
            g_action_in_progress_flag = true;
            g_state_timeout_start_time = Get_Current_Time_ms();
        }
        g_current_machine_state = STATE_DEAL_P1_LOWER_TRAY_WAIT_COMPLETION;
        break;

    case STATE_DEAL_P1_LOWER_TRAY_WAIT_COMPLETION:
        if (Is_Tray_At_Bottom())
        {
            g_action_in_progress_flag = false; // 清除动作标志
            g_current_machine_state = STATE_DEAL_P1_LOAD_TOP_START;
        }
        else if (Get_Current_Time_ms() - g_state_timeout_start_time > TIMEOUT_DURATION_MS)
        {
            System_Error_Handler("Timeout: Deal P1 Lower Tray");
            g_current_machine_state = STATE_ERROR;
        }
        break;

    // ========= 发牌阶段 - 第2步：再送两张牌 (上层) =========
    case STATE_DEAL_P1_LOAD_TOP_START:
        expected_ir1_target_count = g_ir1_raw_count + 1;
        expected_ir2_target_count = g_ir2_raw_count + 1;
        Conveyor_Start(1);
        Conveyor_Start(2);
        g_state_timeout_start_time = Get_Current_Time_ms();
        g_current_machine_state = STATE_DEAL_P1_LOAD_TOP_WAIT_ARRIVAL;
        break;

    case STATE_DEAL_P1_LOAD_TOP_WAIT_ARRIVAL:
        if (g_ir1_raw_count >= expected_ir1_target_count && g_ir2_raw_count >= expected_ir2_target_count)
        {
            Conveyor_Stop(1);
            Conveyor_Stop(2);
            g_current_machine_state = STATE_DEAL_P1_RAISE_TRAY_START;
        }
        else if (Get_Current_Time_ms() - g_state_timeout_start_time > TIMEOUT_DURATION_MS)
        {
            System_Error_Handler("Timeout: Deal P1 Load Top Arrival");
            g_current_machine_state = STATE_ERROR;
        }
        break;

    case STATE_DEAL_P1_RAISE_TRAY_START:
        if (!g_action_in_progress_flag)
        {
            Tray_Move_To_Flat();
            g_action_in_progress_flag = true;
            g_state_timeout_start_time = Get_Current_Time_ms();
        }
        g_current_machine_state = STATE_DEAL_P1_RAISE_TRAY_WAIT_COMPLETION;
        break;

    case STATE_DEAL_P1_RAISE_TRAY_WAIT_COMPLETION:
        if (Is_Tray_At_Flat())
        {
            g_action_in_progress_flag = false;
            g_current_machine_state = STATE_JUMP_START; // 发牌阶段完成，进入跳牌
        }
        else if (Get_Current_Time_ms() - g_state_timeout_start_time > TIMEOUT_DURATION_MS)
        {
            System_Error_Handler("Timeout: Deal P1 Raise Tray");
            g_current_machine_state = STATE_ERROR;
        }
        break;

    // ========= 跳牌阶段 =========
    case STATE_JUMP_START:
        g_jump_phase_lift_count = 0; // 准备升第一组（两张）
        g_current_machine_state = STATE_JUMP_LIFT_TWO_START;
        break;

    case STATE_JUMP_LIFT_TWO_START: // 第一次升两张
        if (!g_action_in_progress_flag)
        {
            LiftMechanism_Lift_Two_Tiles_For_Jump();
            g_action_in_progress_flag = true;
            g_state_timeout_start_time = Get_Current_Time_ms();
        }
        g_current_machine_state = STATE_JUMP_LIFT_TWO_WAIT_COMPLETION;
        break;

    case STATE_JUMP_LIFT_TWO_WAIT_COMPLETION:
        if (Is_LiftMechanism_Lift_Two_Tiles_Done())
        {
            g_action_in_progress_flag = false;
            expected_ir1_target_count = g_ir1_raw_count + 1; // 期望被取走1
            expected_ir2_target_count = g_ir2_raw_count + 1; // 期望被取走2
            g_state_timeout_start_time = Get_Current_Time_ms();
            g_current_machine_state = STATE_JUMP_AWAIT_TAKE_TWO;
        }
        else if (Get_Current_Time_ms() - g_state_timeout_start_time > TIMEOUT_DURATION_MS)
        {
            System_Error_Handler("Timeout: Jump Lift Two Tiles");
            g_current_machine_state = STATE_ERROR;
        }
        break;

    case STATE_JUMP_AWAIT_TAKE_TWO:
        if (g_ir1_raw_count >= expected_ir1_target_count && g_ir2_raw_count >= expected_ir2_target_count)
        {
            g_jump_phase_lift_count = 1; // 标记两张已被取走
            g_current_machine_state = STATE_JUMP_CHECK_NEXT;
        }
        else if (Get_Current_Time_ms() - g_state_timeout_start_time > TIMEOUT_DURATION_MS)
        {
            // 玩家长时间未取牌，可以选择降下或保持，这里示例为报错
            System_Error_Handler("Timeout: Jump Await Take Two");
            g_current_machine_state = STATE_ERROR; // 或者回到一个安全状态，比如降下牌
        }
        break;

    case STATE_JUMP_CHECK_NEXT:    // 检查是否继续升单张
        g_jump_phase_lift_count++; // 计数器递增 (2, 3, 4 对应三次单张升牌)
        if (g_jump_phase_lift_count > 4)
        {                                                          // 1次双张 + 3次单张 完成
            g_current_machine_state = STATE_NORMAL_LIFT_ONE_START; // 进入常规行牌
        }
        else
        {
            // 决定从哪边升牌 (这里简单轮流，你可以设计更复杂的逻辑)
            // g_jump_phase_lift_count = 2 -> conveyor 1
            // g_jump_phase_lift_count = 3 -> conveyor 2
            // g_jump_phase_lift_count = 4 -> conveyor 1
            if (g_jump_phase_lift_count == 3)
            {
                g_tile_lifted_from_conveyor_id = 2;
            }
            else
            {
                g_tile_lifted_from_conveyor_id = 1;
            }
            g_current_machine_state = STATE_JUMP_LIFT_ONE_START;
        }
        break;

    case STATE_JUMP_LIFT_ONE_START: // 后续升一张
        if (!g_action_in_progress_flag)
        {
            LiftMechanism_Lift_One_Tile(g_tile_lifted_from_conveyor_id);
            g_action_in_progress_flag = true;
            g_state_timeout_start_time = Get_Current_Time_ms();
        }
        g_current_machine_state = STATE_JUMP_LIFT_ONE_WAIT_COMPLETION;
        break;

    case STATE_JUMP_LIFT_ONE_WAIT_COMPLETION:
        if (Is_LiftMechanism_Lift_One_Tile_Done())
        {
            g_action_in_progress_flag = false;
            if (g_tile_lifted_from_conveyor_id == 1)
            {
                expected_ir1_target_count = g_ir1_raw_count + 1;
            }
            else
            { // id == 2
                expected_ir2_target_count = g_ir2_raw_count + 1;
            }
            g_state_timeout_start_time = Get_Current_Time_ms();
            g_current_machine_state = STATE_JUMP_AWAIT_TAKE_ONE;
        }
        else if (Get_Current_Time_ms() - g_state_timeout_start_time > TIMEOUT_DURATION_MS)
        {
            System_Error_Handler("Timeout: Jump Lift One Tile");
            g_current_machine_state = STATE_ERROR;
        }
        break;

    case STATE_JUMP_AWAIT_TAKE_ONE:
        bool taken = false;
        if (g_tile_lifted_from_conveyor_id == 1 && g_ir1_raw_count >= expected_ir1_target_count)
        {
            taken = true;
        }
        else if (g_tile_lifted_from_conveyor_id == 2 && g_ir2_raw_count >= expected_ir2_target_count)
        {
            taken = true;
        }

        if (taken)
        {
            g_current_machine_state = STATE_JUMP_CHECK_NEXT; // 回去检查是否还有下一轮单张升牌
        }
        else if (Get_Current_Time_ms() - g_state_timeout_start_time > TIMEOUT_DURATION_MS)
        {
            System_Error_Handler("Timeout: Jump Await Take One");
            g_current_machine_state = STATE_ERROR;
        }
        break;

    // ========= 常规行牌阶段 =========
    case STATE_NORMAL_LIFT_ONE_START:
        // 决定从哪边升牌，例如，可以设计一个变量记录上次是哪边补牌，然后升另一边，或固定顺序。
        // 这里简单假设总是先尝试从传送带1升牌，如果没牌则从2（实际需要更完善的牌管理逻辑）
        g_tile_lifted_from_conveyor_id = 1; // 假设默认升传送带1的牌
        // TODO: 可能需要检查牌堆是否有牌的逻辑

        if (!g_action_in_progress_flag)
        {
            LiftMechanism_Lift_One_Tile(g_tile_lifted_from_conveyor_id);
            g_action_in_progress_flag = true;
            g_state_timeout_start_time = Get_Current_Time_ms();
        }
        g_current_machine_state = STATE_NORMAL_LIFT_ONE_WAIT_COMPLETION;
        break;

    case STATE_NORMAL_LIFT_ONE_WAIT_COMPLETION:
        if (Is_LiftMechanism_Lift_One_Tile_Done())
        {
            g_action_in_progress_flag = false;
            if (g_tile_lifted_from_conveyor_id == 1)
            {
                expected_ir1_target_count = g_ir1_raw_count + 1;
            }
            else
            { // id == 2
                expected_ir2_target_count = g_ir2_raw_count + 1;
            }
            g_state_timeout_start_time = Get_Current_Time_ms();
            g_current_machine_state = STATE_NORMAL_AWAIT_TAKE_ONE;
        }
        else if (Get_Current_Time_ms() - g_state_timeout_start_time > TIMEOUT_DURATION_MS)
        {
            System_Error_Handler("Timeout: Normal Lift One Tile");
            g_current_machine_state = STATE_ERROR;
        }
        break;

    case STATE_NORMAL_AWAIT_TAKE_ONE:
        bool normal_taken = false;
        if (g_tile_lifted_from_conveyor_id == 1 && g_ir1_raw_count >= expected_ir1_target_count)
        {
            normal_taken = true;
        }
        else if (g_tile_lifted_from_conveyor_id == 2 && g_ir2_raw_count >= expected_ir2_target_count)
        {
            normal_taken = true;
        }

        if (normal_taken)
        {
            g_current_machine_state = STATE_NORMAL_LOWER_TRAY_START;
        }
        else if (Get_Current_Time_ms() - g_state_timeout_start_time > TIMEOUT_DURATION_MS)
        {
            System_Error_Handler("Timeout: Normal Await Take One");
            g_current_machine_state = STATE_ERROR;
        }
        break;

    case STATE_NORMAL_LOWER_TRAY_START:
        if (!g_action_in_progress_flag)
        {
            Tray_Move_Down();
            g_action_in_progress_flag = true;
            g_state_timeout_start_time = Get_Current_Time_ms();
        }
        g_current_machine_state = STATE_NORMAL_LOWER_TRAY_WAIT_COMPLETION;
        break;

    case STATE_NORMAL_LOWER_TRAY_WAIT_COMPLETION:
        if (Is_Tray_At_Bottom())
        {
            g_action_in_progress_flag = false;
            g_current_machine_state = STATE_NORMAL_RELOAD_ONE_START;
        }
        else if (Get_Current_Time_ms() - g_state_timeout_start_time > TIMEOUT_DURATION_MS)
        {
            System_Error_Handler("Timeout: Normal Lower Tray");
            g_current_machine_state = STATE_ERROR;
        }
        break;

    case STATE_NORMAL_RELOAD_ONE_START: // 补牌到之前被取走牌的那一路
        if (g_tile_lifted_from_conveyor_id == 1)
        {
            expected_ir1_target_count = g_ir1_raw_count + 1;
        }
        else
        { // id == 2
            expected_ir2_target_count = g_ir2_raw_count + 1;
        }
        Conveyor_Start(g_tile_lifted_from_conveyor_id);
        g_state_timeout_start_time = Get_Current_Time_ms();
        g_current_machine_state = STATE_NORMAL_RELOAD_ONE_WAIT_ARRIVAL;
        break;

    case STATE_NORMAL_RELOAD_ONE_WAIT_ARRIVAL:
        bool reloaded = false;
        if (g_tile_lifted_from_conveyor_id == 1 && g_ir1_raw_count >= expected_ir1_target_count)
        {
            reloaded = true;
        }
        else if (g_tile_lifted_from_conveyor_id == 2 && g_ir2_raw_count >= expected_ir2_target_count)
        {
            reloaded = true;
        }

        if (reloaded)
        {
            Conveyor_Stop(g_tile_lifted_from_conveyor_id);
            g_current_machine_state = STATE_NORMAL_RAISE_TRAY_START;
        }
        else if (Get_Current_Time_ms() - g_state_timeout_start_time > TIMEOUT_DURATION_MS)
        {
            System_Error_Handler("Timeout: Normal Reload One Arrival");
            g_current_machine_state = STATE_ERROR;
        }
        break;

    case STATE_NORMAL_RAISE_TRAY_START:
        if (!g_action_in_progress_flag)
        {
            Tray_Move_To_Flat();
            g_action_in_progress_flag = true;
            g_state_timeout_start_time = Get_Current_Time_ms();
        }
        g_current_machine_state = STATE_NORMAL_RAISE_TRAY_WAIT_COMPLETION;
        break;

    case STATE_NORMAL_RAISE_TRAY_WAIT_COMPLETION:
        if (Is_Tray_At_Flat())
        {
            g_action_in_progress_flag = false;
            g_current_machine_state = STATE_NORMAL_LIFT_ONE_START; // 回到常规行牌的升牌步骤
        }
        else if (Get_Current_Time_ms() - g_state_timeout_start_time > TIMEOUT_DURATION_MS)
        {
            System_Error_Handler("Timeout: Normal Raise Tray");
            g_current_machine_state = STATE_ERROR;
        }
        break;

    // ========= 错误状态 =========
    case STATE_ERROR:
        // 在这里执行错误处理逻辑，例如：
        // 停止所有电机
        Conveyor_Stop(1);
        Conveyor_Stop(2);
        // 点亮错误指示灯
        // 等待管理员干预或重启
        // 可以考虑添加一个从错误状态恢复的机制，或者只能通过重启恢复
        break;

    default:
        // 未知状态，非常规情况，应记录并转到错误状态或空闲状态
        System_Error_Handler("Unknown State Encountered");
        g_current_machine_state = STATE_ERROR;
        break;
    }

    // 3. 低优先级任务 (如果需要，例如LED闪烁，按键扫描等不影响主逻辑的任务)
    // ...
}