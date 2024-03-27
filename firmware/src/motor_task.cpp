#include <SimpleFOC.h>

#include "motor_task.h"

#include "motors/motor_config.h"
#include "util.h"

#include "esp_now_task.h"

#include "my_config.h"

static const float DEAD_ZONE_DETENT_PERCENT = 0.2;
static const float DEAD_ZONE_RAD = 1 * _PI / 180;

static const float IDLE_VELOCITY_EWMA_ALPHA = 0.001;
static const float IDLE_VELOCITY_RAD_PER_SEC = 0.05;
static const uint32_t IDLE_CORRECTION_DELAY_MILLIS = 500;
static const float IDLE_CORRECTION_MAX_ANGLE_RAD = 5 * PI / 180;
static const float IDLE_CORRECTION_RATE_ALPHA = 0.0005;

MotorTask::MotorTask(const uint8_t task_core, Configuration &configuration) : Task("Motor", 2048 * 2, 1, task_core), configuration_(configuration)
{
    queue_ = xQueueCreate(5, sizeof(Command));
    assert(queue_ != NULL);
}

MotorTask::~MotorTask() {}

MagneticSensorSPI encoder = MagneticSensorSPI(AS5047_SPI, AS4057_HSPI_SS);
SPIClass SPI_2(HSPI);

static uint8_t music_box_sta = 0;
static uint8_t spin_sta = 0;
static float spin_torque = 0;

uint8_t get_music_box_sta()
{
    return music_box_sta;
}

void MotorTask::run()
{
    SPI_2.begin(AS4057_HSPI_SCLK, AS4057_HSPI_MISO, AS4057_HSPI_MOSI, AS4057_HSPI_SS); // SCLK, MISO, MOSI, SS
    encoder.init(&SPI_2);

    driver.voltage_power_supply = 4;
    driver.init();

    motor.linkDriver(&driver);

    motor.controller = MotionControlType::torque;
    motor.voltage_limit = FOC_VOLTAGE_LIMIT;
    motor.linkSensor(&encoder);

    // Not actually using the velocity loop built into SimpleFOC; but I'm using those PID variables
    // to run PID for torque (and SimpleFOC studio supports updating them easily over serial for tuning)
    motor.PID_velocity.P = FOC_PID_P;
    motor.PID_velocity.I = FOC_PID_I;
    motor.PID_velocity.D = FOC_PID_D;
    motor.PID_velocity.output_ramp = FOC_PID_OUTPUT_RAMP;
    motor.PID_velocity.limit = FOC_PID_LIMIT;

#ifdef FOC_LPF
    motor.LPF_angle.Tf = FOC_LPF;
    // motor.LPF_velocity.Tf = 0.01; // to set it to 10ms
#endif

    motor.init();

    encoder.update();
    delay(10);

    PB_PersistentConfiguration c = configuration_.get();

    motor.pole_pairs = c.motor.calibrated ? c.motor.pole_pairs : 7;

    motor.initFOC();

    motor.monitor_downsample = 0; // disable monitor at first - optional

    float current_detent_center = motor.shaft_angle;
    PB_SmartKnobConfig config = {
        .position = 0,
        .sub_position_unit = 0,
        .position_nonce = 0,
        .min_position = 0,
        .max_position = 1,
        .position_width_radians = 60 * _PI / 180,
        .detent_strength_unit = 0,
    };

    int32_t last_position = 0;
    int32_t current_position = 0;
    float current_pos = 0;
    float latest_sub_position_unit = 0;

    float idle_check_velocity_ewma = 0;
    uint32_t last_idle_start = 0;
    uint32_t last_publish = 0;
    uint32_t last_publish_mqtt = 0;

    float last_set_point = 0;

    while (1)
    {
        motor.loopFOC();

        // Check queue for pending requests from other tasks
        Command command;
        if (xQueueReceive(queue_, &command, 0) == pdTRUE)
        {
            switch (command.command_type)
            {
            case CommandType::CALIBRATE:
                calibrate();
                break;
            case CommandType::CONFIG:
            {
                current_pos = 0;
                music_box_sta = 0;
                spin_sta = 0;

                // Check new config for validity
                PB_SmartKnobConfig &new_config = command.data.config;
                if (new_config.detent_strength_unit < 0)
                {
                    log("Ignoring invalid config: detent_strength_unit cannot be negative");
                    break;
                }
                if (new_config.endstop_strength_unit < 0)
                {
                    log("Ignoring invalid config: endstop_strength_unit cannot be negative");
                    break;
                }
                if (new_config.snap_point < 0.5)
                {
                    log("Ignoring invalid config: snap_point must be >= 0.5 for stability");
                    break;
                }
                if (new_config.detent_positions_count > COUNT_OF(new_config.detent_positions))
                {
                    log("Ignoring invalid config: detent_positions_count is too large");
                    break;
                }
                if (new_config.snap_point_bias < 0)
                {
                    log("Ignoring invalid config: snap_point_bias cannot be negative or there is risk of instability");
                    break;
                }

                // Change haptic input mode
                bool position_updated = false;
                if (new_config.position != config.position || new_config.sub_position_unit != config.sub_position_unit || new_config.position_nonce != config.position_nonce)
                {
                    log("applying position change");
                    current_position = new_config.position;
                    position_updated = true;
                }

                if (new_config.min_position <= new_config.max_position)
                {
                    // Only check bounds if min/max indicate bounds are active (min >= max)
                    if (current_position < new_config.min_position)
                    {
                        current_position = new_config.min_position;
                        log("adjusting position to min");
                    }
                    else if (current_position > new_config.max_position)
                    {
                        current_position = new_config.max_position;
                        log("adjusting position to max");
                    }
                }

                if (position_updated || new_config.position_width_radians != config.position_width_radians)
                {
                    log("adjusting detent center");
                    float new_sub_position = position_updated ? new_config.sub_position_unit : latest_sub_position_unit;
#if SK_INVERT_ROTATION
                    float shaft_angle = -motor.shaft_angle;
#else
                    float shaft_angle = motor.shaft_angle;
#endif
                    current_detent_center = shaft_angle + new_sub_position * new_config.position_width_radians;
                }

                config = new_config;
                log("Got new config");

                if (strncmp(config.text, "Spin", 4) == 0)
                {
                    spin_sta = 0xff;
                }

                last_position = 0;

                // Update derivative factor of torque controller based on detent width.
                // If the D factor is large on coarse detents, the motor ends up making noise because the P&D factors amplify the noise from the sensor.
                // This is a piecewise linear function so that fine detents (small width) get a higher D factor and coarse detents get a small D factor.
                // Fine detents need a nonzero D factor to artificially create "clicks" each time a new value is reached (the P factor is small
                // for fine detents due to the smaller angular errors, and the existing P factor doesn't work well for very small angle changes (easy to
                // get runaway due to sensor noise & lag)).
                // TODO: consider eliminating this D factor entirely and just "play" a hardcoded haptic "click" (e.g. a quick burst of torque in each
                // direction) whenever the position changes when the detent width is too small for the P factor to work well.
                const float derivative_lower_strength = config.detent_strength_unit * 0.08;
                const float derivative_upper_strength = config.detent_strength_unit * 0.02;
                const float derivative_position_width_lower = radians(3);
                const float derivative_position_width_upper = radians(8);
                const float raw = derivative_lower_strength + (derivative_upper_strength - derivative_lower_strength) / (derivative_position_width_upper - derivative_position_width_lower) * (config.position_width_radians - derivative_position_width_lower);
                // When there are intermittent detents (set via detent_positions), disable derivative factor as this adds extra "clicks" when nearing
                // a detent.
                motor.PID_velocity.D = config.detent_positions_count > 0 ? 0 : CLAMP(raw, min(derivative_lower_strength, derivative_upper_strength), max(derivative_lower_strength, derivative_upper_strength));
                break;
            }
            case CommandType::HAPTIC:
            {
                current_pos = 0;
                music_box_sta = 0;
                spin_sta = 0;

                // Play a hardcoded haptic "click"
                // 按下 和 释放 都抖动电机
                float strength = command.data.haptic.press ? 5 : 1.5;

                motor.move(strength);
                for (uint8_t i = 0; i < 3; i++)
                {
                    motor.loopFOC();
                    delay(1);
                }

                motor.move(-strength);
                for (uint8_t i = 0; i < 3; i++)
                {
                    motor.loopFOC();
                    delay(1);
                }

                motor.move(0);
                motor.loopFOC();
                break;
            }
            }
        }

        // If we are not moving and we're close to the center (but not exactly there), slowly adjust the centerpoint to match the current position
        idle_check_velocity_ewma = motor.shaft_velocity * IDLE_VELOCITY_EWMA_ALPHA + idle_check_velocity_ewma * (1 - IDLE_VELOCITY_EWMA_ALPHA);
        if (fabsf(idle_check_velocity_ewma) > IDLE_VELOCITY_RAD_PER_SEC)
        {
            last_idle_start = 0;
        }
        else
        {
            if (last_idle_start == 0)
            {
                last_idle_start = millis();
            }
        }
        if (last_idle_start > 0 && millis() - last_idle_start > IDLE_CORRECTION_DELAY_MILLIS && fabsf(motor.shaft_angle - current_detent_center) < IDLE_CORRECTION_MAX_ANGLE_RAD)
        {
            current_detent_center = motor.shaft_angle * IDLE_CORRECTION_RATE_ALPHA + current_detent_center * (1 - IDLE_CORRECTION_RATE_ALPHA);
        }

        // Check where we are relative to the current nearest detent; update our position if we've moved far enough to snap to another detent
        float angle_to_detent_center = motor.shaft_angle - current_detent_center;
#if SK_INVERT_ROTATION
        angle_to_detent_center = -motor.shaft_angle - current_detent_center;
#endif

        float snap_point_radians = config.position_width_radians * config.snap_point;
        float bias_radians = config.position_width_radians * config.snap_point_bias;
        float snap_point_radians_decrease = snap_point_radians + (current_position <= 0 ? bias_radians : -bias_radians);
        float snap_point_radians_increase = -snap_point_radians + (current_position >= 0 ? -bias_radians : bias_radians);

        int32_t num_positions = config.max_position - config.min_position + 1;
        if (angle_to_detent_center > snap_point_radians_decrease && (num_positions <= 0 || current_position > config.min_position))
        {
            current_detent_center += config.position_width_radians;
            angle_to_detent_center -= config.position_width_radians;
            current_position--;
        }
        else if (angle_to_detent_center < snap_point_radians_increase && (num_positions <= 0 || current_position < config.max_position))
        {
            current_detent_center -= config.position_width_radians;
            angle_to_detent_center += config.position_width_radians;
            current_position++;
        }

        latest_sub_position_unit = -angle_to_detent_center / config.position_width_radians;

        float dead_zone_adjustment = CLAMP(
            angle_to_detent_center,
            fmaxf(-config.position_width_radians * DEAD_ZONE_DETENT_PERCENT, -DEAD_ZONE_RAD),
            fminf(config.position_width_radians * DEAD_ZONE_DETENT_PERCENT, DEAD_ZONE_RAD));

        bool out_of_bounds = num_positions > 0 && ((angle_to_detent_center > 0 && current_position == config.min_position) || (angle_to_detent_center < 0 && current_position == config.max_position));
        motor.PID_velocity.limit = 10; // out_of_bounds ? 10 : 3;
        motor.PID_velocity.P = out_of_bounds ? config.endstop_strength_unit * 4 : config.detent_strength_unit * 4;

        // Apply motor torque based on our angle to the nearest detent (detent strength, etc is handled by the PID_velocity parameters)
        if (fabsf(motor.shaft_velocity) > 60)
        {
            // Don't apply torque if velocity is too high (helps avoid positive feedback loop/runaway)
            motor.move(0);
        }
        else
        {
            float input = -angle_to_detent_center + dead_zone_adjustment;
            if (!out_of_bounds && config.detent_positions_count > 0)
            {
                bool in_detent = false;
                for (uint8_t i = 0; i < config.detent_positions_count; i++)
                {
                    if (config.detent_positions[i] == current_position)
                    {
                        in_detent = true;
                        break;
                    }
                }
                if (!in_detent)
                {
                    input = 0;
                }
            }

            float torque = motor.PID_velocity(input);
#if SK_INVERT_ROTATION
            torque = -torque;
#endif
            if (strncmp(config.text, "EspNow", 6) == 0)
            {
                float thatMotor = espNowGetPeerMotor();
                if (thatMotor == 0x7FFFFFFE)
                {
                    thatMotor = motor.shaft_angle;
                }

                float setPoint = 1 * (thatMotor - motor.shaft_angle);
                if (setPoint >= motor.voltage_limit)
                    setPoint = motor.voltage_limit;
                else if (setPoint <= -motor.voltage_limit)
                    setPoint = -motor.voltage_limit;
                motor.move(setPoint);
            }
            else if (strncmp(config.text, "MusicBox", 8) == 0)
            {
                // TODO: 1秒位置未改变, 切换到缓慢归零模式
                if (current_position > last_position)
                {
                    last_set_point = millis();
                    music_box_sta = 0;
                    current_pos = 0;
                }
                else if (current_position == 100)
                {
                    if (millis() - last_set_point >= 1000)
                    {
                        log("New mode");
                        last_set_point = millis();
                        if (music_box_sta == 0)
                        {
                            current_pos = current_position;
                            music_box_sta = 1;
                        }
                    }
                }

                if (music_box_sta != 0)
                {
                    if (current_pos > 0)
                    {
                        current_pos -= MUSICBOX_STEP;  // 值越小,转的圈数越多
                        torque = (-(MUSICBOX_TORQUE)); // 力矩
                        current_position = current_pos;
                        if (current_position <= 0)
                        {
                            torque = 0;
                            current_position = 0;
                            motor.move(torque);
                        }
                    }
                    else
                    {
                        music_box_sta = 0;
                        torque = 0;
                        motor.move(torque);
                    }
                }

                last_position = current_position;
            }
            else if (strncmp(config.text, "Spin", 4) == 0)
            {
                // 设置一个初始速度
                if (motor.shaft_velocity < -10 && spin_sta == 0)
                {
                    spin_sta = 1;
                    spin_torque = (-(SPIN_TORQUE));
                }
                else if (motor.shaft_velocity > 10 && spin_sta == 0)
                {
                    spin_sta = 2;
                    spin_torque = SPIN_TORQUE;
                }

                // 在这个速度范围内，停下来
                if ((fabs(motor.shaft_velocity) <= 5) && (spin_sta != 0xff) && (spin_sta != 0))
                {
                    torque = 0;
                    spin_torque = 0;
                    spin_sta = 0;
                }

                // 速度逐渐降低
                if (spin_sta == 1)
                {
                    spin_torque += SPIN_STEP;
                    if (spin_torque >= 0)
                        spin_torque = 0;
                    torque = spin_torque;
                }
                else if (spin_sta == 2)
                {
                    spin_torque -= SPIN_STEP;
                    if (spin_torque <= 0)
                        spin_torque = 0;
                    torque = spin_torque;
                }

                if (spin_sta == 0xff && torque == 0)
                {
                    spin_sta = 0x00;
                }
            }

            if (strncmp(config.text, "EspNow", 6) != 0)
                motor.move(torque);
        }

        // Publish current status to other registered tasks periodically
        if (millis() - last_publish > 5)
        {
            publish({
                .current_position = current_position,
                .sub_position_unit = latest_sub_position_unit,
                .has_config = true,
                .config = config,
                .current_angle = motor.shaft_angle,
            });
            last_publish = millis();
        }

        if (millis() - last_publish_mqtt >= 100)
        {
            // 将当前电机角度发给对方
            if (strncmp(config.text, "EspNow", 6) == 0)
            {
                espNowSendData(motor.shaft_angle);
            }
            last_publish_mqtt = millis();
        }

        delay(1);
    }
}

/// @brief 设置电机模式
/// @param config
void MotorTask::setConfig(const PB_SmartKnobConfig &config)
{
    Command command = {
        .command_type = CommandType::CONFIG,
        .data = {
            .config = config,
        }};
    xQueueSend(queue_, &command, portMAX_DELAY);
}

/// @brief 发送触觉反馈指令
/// @param press
/// @return void
void MotorTask::playHaptic(bool press)
{
    Command command = {
        .command_type = CommandType::HAPTIC,
        .data = {
            .haptic = {
                .press = press,
            },
        }};
    xQueueSend(queue_, &command, portMAX_DELAY);
}

/// @brief 发送校准指令
/// @param void
/// @return void
void MotorTask::runCalibration()
{
    Command command = {
        .command_type = CommandType::CALIBRATE,
        .data = {
            .unused = 0,
        }};
    xQueueSend(queue_, &command, portMAX_DELAY);
}

// 变量名: listeners_
// 类型: std::vector<QueueHandle_t>
// std::vector: 向量容器,存储 QueueHandle_t 类型的元素
// std::vector<QueueHandle_t> listeners_;
void MotorTask::addListener(QueueHandle_t queue)
{
    // push_back() 是 std::vector的成员函数, 用于将一个新元素添加到向量末尾
    listeners_.push_back(queue);
}

void MotorTask::publish(const PB_SmartKnobState &state)
{
    // 遍历 listeners_ 中的所有元素
    // 此处: 向所有 监听器 发送消息(消息队列)
    // 关键字auto: 自动推断元素类型
    for (auto listener : listeners_)
    {
        xQueueOverwrite(listener, &state);
    }
}

/// @brief 校准电机
/// @param void
/// @return void
void MotorTask::calibrate()
{
    // SimpleFOC is supposed to be able to determine this automatically (if you omit params to initFOC), but
    // it seems to have a bug (or I've misconfigured it) that gets both the offset and direction very wrong!
    // So this value is based on experimentation.
    // TODO: dig into SimpleFOC calibration and find/fix the issue

    log("\n\n\nStarting calibration, please DO NOT TOUCH MOTOR until complete!");
    delay(1000);

    motor.controller = MotionControlType::angle_openloop;
    motor.pole_pairs = 1;
    // motor.initFOC(0, Direction::CW);
    motor.initFOC();

    float a = 0;

    motor.voltage_limit = FOC_VOLTAGE_LIMIT;
    motor.move(a);

    // #### Determine direction motor rotates relative to angle sensor
    for (uint8_t i = 0; i < 200; i++)
    {
        encoder.update();
        motor.move(a);
        delay(1);
    }
    float start_sensor = encoder.getAngle();

    for (; a < 3 * _2PI; a += 0.01)
    {
        encoder.update();
        motor.move(a);
        delay(1);
    }

    for (uint8_t i = 0; i < 200; i++)
    {
        encoder.update();
        delay(1);
    }
    float end_sensor = encoder.getAngle();

    motor.voltage_limit = 0;
    motor.move(a);

    log("");

    float movement_angle = fabsf(end_sensor - start_sensor);
    if (movement_angle < radians(30) || movement_angle > radians(180))
    {
        snprintf(buf_, sizeof(buf_), "ERROR! Unexpected sensor change: start=%.2f end=%.2f", start_sensor, end_sensor);
        log(buf_);
        return;
    }

    log("Sensor measures positive for positive motor rotation:");
    if (end_sensor > start_sensor)
    {
        log("YES, Direction=CW");
        // motor.initFOC(0, Direction::CW);
        motor.initFOC();
    }
    else
    {
        log("NO, Direction=CCW");
        // motor.initFOC(0, Direction::CCW);
        motor.initFOC();
    }
    snprintf(buf_, sizeof(buf_), "  (start was %.1f, end was %.1f)", start_sensor, end_sensor);
    log(buf_);

    // #### Determine pole-pairs
    // Rotate 20 electrical revolutions and measure mechanical angle traveled, to calculate pole-pairs
    uint8_t electrical_revolutions = 20;
    snprintf(buf_, sizeof(buf_), "Going to measure %d electrical revolutions...", electrical_revolutions);
    log(buf_);
    motor.voltage_limit = FOC_VOLTAGE_LIMIT;
    motor.move(a);
    log("Going to electrical zero...");
    float destination = a + _2PI;
    for (; a < destination; a += 0.03)
    {
        encoder.update();
        motor.move(a);
        delay(1);
    }
    log("pause..."); // Let momentum settle...
    for (uint16_t i = 0; i < 1000; i++)
    {
        encoder.update();
        delay(1);
    }
    log("Measuring...");

    start_sensor = motor.sensor_direction * encoder.getAngle();
    destination = a + electrical_revolutions * _2PI;
    for (; a < destination; a += 0.03)
    {
        encoder.update();
        motor.move(a);
        delay(1);
    }
    for (uint16_t i = 0; i < 1000; i++)
    {
        encoder.update();
        motor.move(a);
        delay(1);
    }
    end_sensor = motor.sensor_direction * encoder.getAngle();
    motor.voltage_limit = 0;
    motor.move(a);

    if (fabsf(motor.shaft_angle - motor.target) > 1 * PI / 180)
    {
        log("ERROR: motor did not reach target!");
        while (1)
        {
        }
    }

    float electrical_per_mechanical = electrical_revolutions * _2PI / (end_sensor - start_sensor);
    snprintf(buf_, sizeof(buf_), "Electrical angle / mechanical angle (i.e. pole pairs) = %.2f", electrical_per_mechanical);
    log(buf_);

    if (electrical_per_mechanical < 3 || electrical_per_mechanical > 12)
    {
        snprintf(buf_, sizeof(buf_), "ERROR! Unexpected calculated pole pairs: %.2f", electrical_per_mechanical);
        log(buf_);
        return;
    }

    int measured_pole_pairs = (int)round(electrical_per_mechanical);
    snprintf(buf_, sizeof(buf_), "Pole pairs set to %d", measured_pole_pairs);
    log(buf_);

    delay(1000);

    // #### Determine mechanical offset to electrical zero
    // Measure mechanical angle at every electrical zero for several revolutions
    motor.voltage_limit = FOC_VOLTAGE_LIMIT;
    motor.move(a);
    float offset_x = 0;
    float offset_y = 0;
    float destination1 = (floor(a / _2PI) + measured_pole_pairs / 2.) * _2PI;
    float destination2 = (floor(a / _2PI)) * _2PI;
    for (; a < destination1; a += 0.4)
    {
        motor.move(a);
        delay(100);
        for (uint8_t i = 0; i < 100; i++)
        {
            encoder.update();
            delay(1);
        }
        float real_electrical_angle = _normalizeAngle(a);
        float measured_electrical_angle = _normalizeAngle((float)(motor.sensor_direction * measured_pole_pairs) * encoder.getMechanicalAngle() - 0);

        float offset_angle = measured_electrical_angle - real_electrical_angle;
        offset_x += cosf(offset_angle);
        offset_y += sinf(offset_angle);

        snprintf(buf_, sizeof(buf_), "%.2f, %.2f, %.2f", degrees(real_electrical_angle), degrees(measured_electrical_angle), degrees(_normalizeAngle(offset_angle)));
        log(buf_);
    }
    for (; a > destination2; a -= 0.4)
    {
        motor.move(a);
        delay(100);
        for (uint8_t i = 0; i < 100; i++)
        {
            encoder.update();
            delay(1);
        }
        float real_electrical_angle = _normalizeAngle(a);
        float measured_electrical_angle = _normalizeAngle((float)(motor.sensor_direction * measured_pole_pairs) * encoder.getMechanicalAngle() - 0);

        float offset_angle = measured_electrical_angle - real_electrical_angle;
        offset_x += cosf(offset_angle);
        offset_y += sinf(offset_angle);

        snprintf(buf_, sizeof(buf_), "%.2f, %.2f, %.2f", degrees(real_electrical_angle), degrees(measured_electrical_angle), degrees(_normalizeAngle(offset_angle)));
        log(buf_);
    }
    motor.voltage_limit = 0;
    motor.move(a);

    float avg_offset_angle = atan2f(offset_y, offset_x);

    // #### Apply settings
    motor.pole_pairs = measured_pole_pairs;
    motor.zero_electric_angle = avg_offset_angle + _3PI_2;
    motor.voltage_limit = FOC_VOLTAGE_LIMIT;
    motor.controller = MotionControlType::torque;

    log("");
    log("RESULTS:");
    snprintf(buf_, sizeof(buf_), "  ZERO_ELECTRICAL_OFFSET: %.2f", motor.zero_electric_angle);
    log(buf_);
    if (motor.sensor_direction == Direction::CW)
    {
        log("  FOC_DIRECTION: Direction::CW");
    }
    else
    {
        log("  FOC_DIRECTION: Direction::CCW");
    }
    snprintf(buf_, sizeof(buf_), "  MOTOR_POLE_PAIRS: %d", motor.pole_pairs);
    log(buf_);

    log("");
    log("Saving to persistent configuration...");
    PB_MotorCalibration calibration = {
        .calibrated = true,
        .zero_electrical_offset = motor.zero_electric_angle,
        .direction_cw = motor.sensor_direction == Direction::CW,
        .pole_pairs = motor.pole_pairs,
    };

    if (configuration_.setMotorCalibrationAndSave(calibration))
    {
        log("Success!");
    }
}

void MotorTask::checkSensorError()
{
#if SENSOR_TLV
    if (encoder.getAndClearError())
    {
        log("LOCKED!");
    }
#elif SENSOR_MT6701
    MT6701Error error = encoder.getAndClearError();
    if (error.error)
    {
        snprintf(buf_, sizeof(buf_), "CRC error. Received %d; calculated %d", error.received_crc, error.calculated_crc);
        log(buf_);
    }
#endif
}

void MotorTask::setLogger(Logger *logger)
{
    logger_ = logger;
}

void MotorTask::log(const char *msg)
{
    if (logger_ != nullptr)
    {
        logger_->log(msg);
    }
}