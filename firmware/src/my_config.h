#pragma once

#include <stdio.h>
#include <stdlib.h>

#define PRINT_LOG 1    // 打印日志 波特率 9600
#define PRINT_STRAIN 1 // 打印压力传感器数值

#define MY_KNOB_CONFIGS 0 // 0使用原版配置

#define STRAIN_MIN (620000) // 压力传感器没有按压时的值
#define STRAIN_MAX (900000) // 压力传感器按压后的值

// 以下所有配置暂不生效，当前只支持原版SmartKnob的效果，新增音乐盒等功能完善后再上传。
#define MUSICBOX_TORQUE 0.5 // 音乐盒往回转的力矩,转不动可以调大
#define MUSICBOX_STEP 0.001 // 音乐盒往回转的步进,越小转的圈数越多
#define SPIN_TORQUE 5       // 指尖陀螺初始力矩
#define SPIN_STEP 0.001     // 指尖陀螺摩擦系数,越大减速越快

// TODO: 注意---声音是按放入内存的顺序播放的, 比如 0x01表示第一个放入内存的声音文件
#define PRESSED_SOUND 0x01         // 按下时播放的音效序号
#define MENU_SOUND 0x03            // 菜单切换时播放的音效序号
#define CAT_SOUND 0x06             // 猫叫时播放的音效序号
#define MUSICBOX_SPRING_SOUND 0x04 // 音乐盒发条的音效序号
#define MUSICBOX_SOUND 0x05        // 音乐盒的音效序号
#define PRESSED_SOUND7 0x07        // 音乐盒的音效序号
#define REPRESSED_SOUND 0x08       // 音乐盒的音效序号

// ESP-NOW配对时MAC地址: 上电时会打印 MyMac, 替换掉下面的MAC地址
#define MACHINE_NUMBER 2                // 给1号机器下载时设置为1,给2号机器下载时设置为2
#define PEERMAC_ONE "7C:DF:A1:E0:F3:44" // 2号机器下载: 这是1号机器的MAC
#define PEERMAC_TWO "68:B6:B3:3E:08:BC" // 1号机器下载: 这是2号机器的MAC

#define SLEEP_TIME (5 * 1000) // 休眠时间, 时间会大于设定值
