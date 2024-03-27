/* Automatically generated nanopb header */
/* Generated by nanopb-0.4.7 */

#ifndef PB_PB_SMARTKNOB_PB_H_INCLUDED
#define PB_PB_SMARTKNOB_PB_H_INCLUDED
#include <pb.h>

#if PB_PROTO_HEADER_VERSION != 40
#error Regenerate this file with the current version of nanopb generator.
#endif

/* Struct definitions */
/* * Lets the host know that a ToSmartknob message was received and should not be retried. */
typedef struct _PB_Ack
{
   uint32_t nonce;
} PB_Ack;

typedef struct _PB_Log
{
   char msg[256];
} PB_Log;

typedef struct _PB_SmartKnobConfig
{
   /* *
Set the integer position.

Note: in order to make SmartKnobConfig apply idempotently, the current position
will only be set to this value when it changes compared to a previous config (and
NOT compared to the current state!). So by default, if you send a config position
of 5 and the current position is 3, the position may remain at 3 if the config
change to 5 was previously handled. If you need to force a position update, see
position_nonce. */
   int32_t position;
   /* *
Set the fractional position. Typical range: (-snap_point, snap_point).

Actual range is technically unbounded, but in practice this value will be compared
against snap_point on the next control loop, so any value beyond the snap_point will
generally result in an integer position change (unless position is already at a
limit).

Note: idempotency implications noted in the documentation for `position` apply here
as well */
   float sub_position_unit;
   /* *
Position is normally only applied when it changes, but sometimes it's desirable
to reset the position to the same value, so a nonce change can be used to force
the position values to be applied as well.

NOTE: Must be < 256 */
   uint8_t position_nonce;
   /* * Minimum position allowed. */
   int32_t min_position;
   /* *
Maximum position allowed.

If this is the same as min_position, there will only be one allowed position.

If this is less than min_position, bounds will be disabled. */
   int32_t max_position;
   /* * The angular "width" of each position/detent, in radians. */
   float position_width_radians;
   /* *
Strength of detents to apply. Typical range: [0, 1].

A value of 0 disables detents.

Values greater than 1 are not recommended and may lead to unstable behavior. */
   float detent_strength_unit;
   /* *
Strength of endstop torque to apply at min/max bounds. Typical range: [0, 1].

A value of 0 disables endstop torque, but does not make position unbounded, meaning
the knob will not try to return to the valid region. For unbounded rotation, use
min_position and max_position.

Values greater than 1 are not recommended and may lead to unstable behavior. */
   float endstop_strength_unit;
   /* *
Fractional (sub-position) threshold where the position will increment/decrement.
Typical range: (0.5, 1.5).

This defines how hysteresis is applied to positions, which is why values > */
   float snap_point;
   /* *
Arbitrary 50-byte string representing this "config". This can be used to identify major
config/mode changes. The value will be echoed back to the host via a future State's
embedded config field so the host can use this value to determine the mode that was
in effect at the time of the State snapshot instead of having to infer it from the
other config fields. */
   char text[51];
   /* *
For a "magnetic" detent mode - where not all positions should have detents - this
specifies which positions (up to 5) have detents enabled. The knob will feel like it
is "magnetically" attracted to those positions, and will rotate smoothy past all
other positions.

If you want to have more than 5 magnetic detent positions, you will need to dynamically
update this list as the knob is rotated. A recommended approach is to always send the
_nearest_ 5 detent positions, and send a new Config message whenever the list of
positions nearest the current position (as reported via State messages) changes.

This approach enables effectively unbounded detent positions while keeping Config
bounded in size, and is resilient against tightly-packed detents with fast rotation
since multiple detent positions can be sent in advance; a full round-trip Config-State
isn't needed between each detent in order to keep up. */
   pb_size_t detent_positions_count;
   int32_t detent_positions[5];
   /* *
Advanced feature for shifting the defined snap_point away from the center (position 0)
for implementing asymmetric detents. Typical value: 0 (symmetric detent force).

This can be used to create detents that will hold the position when carefully released,
but can be easily disturbed to return "home" towards position 0. */
   float snap_point_bias;
   /* *
Hue (0-255) for all 8 ring LEDs, if supported. Note: this will likely be replaced
with more configurability in a future protocol version. */
   int16_t led_hue;
} PB_SmartKnobConfig;

typedef struct _PB_SmartKnobState
{
   /* * Current integer position of the knob. (Detent resolution is at integer positions) */
   int32_t current_position;
   /* *
Current fractional position. Typically will only range from (-snap_point, snap_point)
since further rotation will result in the integer position changing, but may exceed
those values if snap_point_bias is non-zero, or if the knob is at a bound. When the
knob is at a bound, this value can grow endlessly as the knob is rotated further past
the bound.

When visualizing sub_position_unit, you will likely want to apply a rubber-band easing
function past the bounds; a sublinear relationship will help suggest that a bound has
been reached. */
   float sub_position_unit;
   /* *
Current SmartKnobConfig in effect at the time of this State snapshot.

Beware that this config contains position and sub_position_unit values, not to be
confused with the top level current_position and sub_position_unit values in this State
message. The position values in the embedded config message will almost never be useful
to you; you probably want to be reading the top level values from the State message. */
   bool has_config;
   PB_SmartKnobConfig config;
   /* *
Value that changes each time the knob is pressed. Does not change when a press is released.

Why this press state a "nonce" rather than a simple boolean representing the current
"pressed" state? It makes the protocol more robust to dropped/lost State messages; if
the knob was pressed/released quickly and State messages happened to be dropped during
that time, the press would be completely lost. Using a nonce allows the host to recognize
that a press has taken place at some point even if the State was lost during the press
itself. Is this overkill? Probably, let's revisit in future protocol versions. */
   uint8_t press_nonce;

   float current_angle;
} PB_SmartKnobState;

/* Message FROM the SmartKnob to the host */
typedef struct _PB_FromSmartKnob
{
   uint8_t protocol_version;
   pb_size_t which_payload;
   union
   {
      PB_Ack ack;
      PB_Log log;
      PB_SmartKnobState smartknob_state;
   } payload;
} PB_FromSmartKnob;

typedef struct _PB_RequestState
{
   char dummy_field;
} PB_RequestState;

/* Message TO the Smartknob from the host */
typedef struct _PB_ToSmartknob
{
   uint8_t protocol_version;
   uint32_t nonce;
   pb_size_t which_payload;
   union
   {
      PB_RequestState request_state;
      PB_SmartKnobConfig smartknob_config;
   } payload;
} PB_ToSmartknob;

typedef struct _PB_MotorCalibration
{
   bool calibrated;
   float zero_electrical_offset;
   bool direction_cw;
   uint32_t pole_pairs;
} PB_MotorCalibration;

typedef struct _PB_StrainCalibration
{
   int32_t idle_value;
   int32_t press_delta;
} PB_StrainCalibration;

typedef struct _PB_PersistentConfiguration
{
   uint32_t version;
   bool has_motor;
   PB_MotorCalibration motor;
   bool has_strain;
   PB_StrainCalibration strain;
} PB_PersistentConfiguration;

#ifdef __cplusplus
extern "C"
{
#endif

/* Initializer values for message structs */
#define PB_FromSmartKnob_init_default \
   {                                  \
      0, 0, { PB_Ack_init_default }   \
   }
#define PB_ToSmartknob_init_default             \
   {                                            \
      0, 0, 0, { PB_RequestState_init_default } \
   }
#define PB_Ack_init_default \
   {                        \
      0                     \
   }
#define PB_Log_init_default \
   {                        \
      ""                    \
   }
#define PB_SmartKnobState_init_default                \
   {                                                  \
      0, 0, false, PB_SmartKnobConfig_init_default, 0 \
   }
#define PB_SmartKnobConfig_init_default                       \
   {                                                          \
      0, 0, 0, 0, 0, 0, 0, 0, 0, "", 0, {0, 0, 0, 0, 0}, 0, 0 \
   }
#define PB_RequestState_init_default \
   {                                 \
      0                              \
   }
#define PB_PersistentConfiguration_init_default                                            \
   {                                                                                       \
      0, false, PB_MotorCalibration_init_default, false, PB_StrainCalibration_init_default \
   }
#define PB_MotorCalibration_init_default \
   {                                     \
      0, 0, 0, 0                         \
   }
#define PB_StrainCalibration_init_default \
   {                                      \
      0, 0                                \
   }
#define PB_FromSmartKnob_init_zero \
   {                               \
      0, 0, { PB_Ack_init_zero }   \
   }
#define PB_ToSmartknob_init_zero             \
   {                                         \
      0, 0, 0, { PB_RequestState_init_zero } \
   }
#define PB_Ack_init_zero \
   {                     \
      0                  \
   }
#define PB_Log_init_zero \
   {                     \
      ""                 \
   }
#define PB_SmartKnobState_init_zero                \
   {                                               \
      0, 0, false, PB_SmartKnobConfig_init_zero, 0 \
   }
#define PB_SmartKnobConfig_init_zero                          \
   {                                                          \
      0, 0, 0, 0, 0, 0, 0, 0, 0, "", 0, {0, 0, 0, 0, 0}, 0, 0 \
   }
#define PB_RequestState_init_zero \
   {                              \
      0                           \
   }
#define PB_PersistentConfiguration_init_zero                                         \
   {                                                                                 \
      0, false, PB_MotorCalibration_init_zero, false, PB_StrainCalibration_init_zero \
   }
#define PB_MotorCalibration_init_zero \
   {                                  \
      0, 0, 0, 0                      \
   }
#define PB_StrainCalibration_init_zero \
   {                                   \
      0, 0                             \
   }

/* Field tags (for use in manual encoding/decoding) */
#define PB_Ack_nonce_tag 1
#define PB_Log_msg_tag 1
#define PB_SmartKnobConfig_position_tag 1
#define PB_SmartKnobConfig_sub_position_unit_tag 2
#define PB_SmartKnobConfig_position_nonce_tag 3
#define PB_SmartKnobConfig_min_position_tag 4
#define PB_SmartKnobConfig_max_position_tag 5
#define PB_SmartKnobConfig_position_width_radians_tag 6
#define PB_SmartKnobConfig_detent_strength_unit_tag 7
#define PB_SmartKnobConfig_endstop_strength_unit_tag 8
#define PB_SmartKnobConfig_snap_point_tag 9
#define PB_SmartKnobConfig_text_tag 10
#define PB_SmartKnobConfig_detent_positions_tag 11
#define PB_SmartKnobConfig_snap_point_bias_tag 12
#define PB_SmartKnobConfig_led_hue_tag 13
#define PB_SmartKnobState_current_position_tag 1
#define PB_SmartKnobState_sub_position_unit_tag 2
#define PB_SmartKnobState_config_tag 3
#define PB_SmartKnobState_press_nonce_tag 4
#define PB_FromSmartKnob_protocol_version_tag 1
#define PB_FromSmartKnob_ack_tag 2
#define PB_FromSmartKnob_log_tag 3
#define PB_FromSmartKnob_smartknob_state_tag 4
#define PB_ToSmartknob_protocol_version_tag 1
#define PB_ToSmartknob_nonce_tag 2
#define PB_ToSmartknob_request_state_tag 3
#define PB_ToSmartknob_smartknob_config_tag 4
#define PB_MotorCalibration_calibrated_tag 1
#define PB_MotorCalibration_zero_electrical_offset_tag 2
#define PB_MotorCalibration_direction_cw_tag 3
#define PB_MotorCalibration_pole_pairs_tag 4
#define PB_StrainCalibration_idle_value_tag 1
#define PB_StrainCalibration_press_delta_tag 2
#define PB_PersistentConfiguration_version_tag 1
#define PB_PersistentConfiguration_motor_tag 2
#define PB_PersistentConfiguration_strain_tag 3

/* Struct field encoding specification for nanopb */
#define PB_FromSmartKnob_FIELDLIST(X, a)                        \
   X(a, STATIC, SINGULAR, UINT32, protocol_version, 1)          \
   X(a, STATIC, ONEOF, MESSAGE, (payload, ack, payload.ack), 2) \
   X(a, STATIC, ONEOF, MESSAGE, (payload, log, payload.log), 3) \
   X(a, STATIC, ONEOF, MESSAGE, (payload, smartknob_state, payload.smartknob_state), 4)
#define PB_FromSmartKnob_CALLBACK NULL
#define PB_FromSmartKnob_DEFAULT NULL
#define PB_FromSmartKnob_payload_ack_MSGTYPE PB_Ack
#define PB_FromSmartKnob_payload_log_MSGTYPE PB_Log
#define PB_FromSmartKnob_payload_smartknob_state_MSGTYPE PB_SmartKnobState

#define PB_ToSmartknob_FIELDLIST(X, a)                                              \
   X(a, STATIC, SINGULAR, UINT32, protocol_version, 1)                              \
   X(a, STATIC, SINGULAR, UINT32, nonce, 2)                                         \
   X(a, STATIC, ONEOF, MESSAGE, (payload, request_state, payload.request_state), 3) \
   X(a, STATIC, ONEOF, MESSAGE, (payload, smartknob_config, payload.smartknob_config), 4)
#define PB_ToSmartknob_CALLBACK NULL
#define PB_ToSmartknob_DEFAULT NULL
#define PB_ToSmartknob_payload_request_state_MSGTYPE PB_RequestState
#define PB_ToSmartknob_payload_smartknob_config_MSGTYPE PB_SmartKnobConfig

#define PB_Ack_FIELDLIST(X, a) \
   X(a, STATIC, SINGULAR, UINT32, nonce, 1)
#define PB_Ack_CALLBACK NULL
#define PB_Ack_DEFAULT NULL

#define PB_Log_FIELDLIST(X, a) \
   X(a, STATIC, SINGULAR, STRING, msg, 1)
#define PB_Log_CALLBACK NULL
#define PB_Log_DEFAULT NULL

#define PB_SmartKnobState_FIELDLIST(X, a)              \
   X(a, STATIC, SINGULAR, INT32, current_position, 1)  \
   X(a, STATIC, SINGULAR, FLOAT, sub_position_unit, 2) \
   X(a, STATIC, OPTIONAL, MESSAGE, config, 3)          \
   X(a, STATIC, SINGULAR, UINT32, press_nonce, 4)
#define PB_SmartKnobState_CALLBACK NULL
#define PB_SmartKnobState_DEFAULT NULL
#define PB_SmartKnobState_config_MSGTYPE PB_SmartKnobConfig

#define PB_SmartKnobConfig_FIELDLIST(X, a)                  \
   X(a, STATIC, SINGULAR, INT32, position, 1)               \
   X(a, STATIC, SINGULAR, FLOAT, sub_position_unit, 2)      \
   X(a, STATIC, SINGULAR, UINT32, position_nonce, 3)        \
   X(a, STATIC, SINGULAR, INT32, min_position, 4)           \
   X(a, STATIC, SINGULAR, INT32, max_position, 5)           \
   X(a, STATIC, SINGULAR, FLOAT, position_width_radians, 6) \
   X(a, STATIC, SINGULAR, FLOAT, detent_strength_unit, 7)   \
   X(a, STATIC, SINGULAR, FLOAT, endstop_strength_unit, 8)  \
   X(a, STATIC, SINGULAR, FLOAT, snap_point, 9)             \
   X(a, STATIC, SINGULAR, STRING, text, 10)                 \
   X(a, STATIC, REPEATED, INT32, detent_positions, 11)      \
   X(a, STATIC, SINGULAR, FLOAT, snap_point_bias, 12)       \
   X(a, STATIC, SINGULAR, INT32, led_hue, 13)
#define PB_SmartKnobConfig_CALLBACK NULL
#define PB_SmartKnobConfig_DEFAULT NULL

#define PB_RequestState_FIELDLIST(X, a)

#define PB_RequestState_CALLBACK NULL
#define PB_RequestState_DEFAULT NULL

#define PB_PersistentConfiguration_FIELDLIST(X, a) \
   X(a, STATIC, SINGULAR, UINT32, version, 1)      \
   X(a, STATIC, OPTIONAL, MESSAGE, motor, 2)       \
   X(a, STATIC, OPTIONAL, MESSAGE, strain, 3)
#define PB_PersistentConfiguration_CALLBACK NULL
#define PB_PersistentConfiguration_DEFAULT NULL
#define PB_PersistentConfiguration_motor_MSGTYPE PB_MotorCalibration
#define PB_PersistentConfiguration_strain_MSGTYPE PB_StrainCalibration

#define PB_MotorCalibration_FIELDLIST(X, a)                 \
   X(a, STATIC, SINGULAR, BOOL, calibrated, 1)              \
   X(a, STATIC, SINGULAR, FLOAT, zero_electrical_offset, 2) \
   X(a, STATIC, SINGULAR, BOOL, direction_cw, 3)            \
   X(a, STATIC, SINGULAR, UINT32, pole_pairs, 4)
#define PB_MotorCalibration_CALLBACK NULL
#define PB_MotorCalibration_DEFAULT NULL

#define PB_StrainCalibration_FIELDLIST(X, a)    \
   X(a, STATIC, SINGULAR, INT32, idle_value, 1) \
   X(a, STATIC, SINGULAR, INT32, press_delta, 2)
#define PB_StrainCalibration_CALLBACK NULL
#define PB_StrainCalibration_DEFAULT NULL

   extern const pb_msgdesc_t PB_FromSmartKnob_msg;
   extern const pb_msgdesc_t PB_ToSmartknob_msg;
   extern const pb_msgdesc_t PB_Ack_msg;
   extern const pb_msgdesc_t PB_Log_msg;
   extern const pb_msgdesc_t PB_SmartKnobState_msg;
   extern const pb_msgdesc_t PB_SmartKnobConfig_msg;
   extern const pb_msgdesc_t PB_RequestState_msg;
   extern const pb_msgdesc_t PB_PersistentConfiguration_msg;
   extern const pb_msgdesc_t PB_MotorCalibration_msg;
   extern const pb_msgdesc_t PB_StrainCalibration_msg;

/* Defines for backwards compatibility with code written before nanopb-0.4.0 */
#define PB_FromSmartKnob_fields &PB_FromSmartKnob_msg
#define PB_ToSmartknob_fields &PB_ToSmartknob_msg
#define PB_Ack_fields &PB_Ack_msg
#define PB_Log_fields &PB_Log_msg
#define PB_SmartKnobState_fields &PB_SmartKnobState_msg
#define PB_SmartKnobConfig_fields &PB_SmartKnobConfig_msg
#define PB_RequestState_fields &PB_RequestState_msg
#define PB_PersistentConfiguration_fields &PB_PersistentConfiguration_msg
#define PB_MotorCalibration_fields &PB_MotorCalibration_msg
#define PB_StrainCalibration_fields &PB_StrainCalibration_msg

/* Maximum encoded size of messages (where known) */
#define PB_Ack_size 6
#define PB_FromSmartKnob_size 264
#define PB_Log_size 258
#define PB_MotorCalibration_size 15
#define PB_PersistentConfiguration_size 47
#define PB_RequestState_size 0
#define PB_SmartKnobConfig_size 184
#define PB_SmartKnobState_size 206
#define PB_StrainCalibration_size 22
#define PB_ToSmartknob_size 196

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
