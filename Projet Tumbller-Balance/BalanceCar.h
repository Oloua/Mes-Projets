#include "MsTimer2.h"
#include "KalmanFilter.h"
#include "I2Cdev.h"
#include "MPU6050.h"
#include "Wire.h"

MPU6050 mpu;
KalmanFilter kalmanfilter;

// PID parameters
double kp_balance = 55, kd_balance = 0.75;
double kp_speed = 10, ki_speed = 0.26;
double kp_turn = 2.5, kd_turn = 0.5;

// MPU6050 calibration offsets
double angle_zero = 0;            
double angular_velocity_zero = 0;

// Encoder counters
volatile unsigned long encoder_count_right_a = 0;
volatile unsigned long encoder_count_left_a = 0;

int16_t ax, ay, az, gx, gy, gz;

// Kalman parameters
float dt = 0.005, Q_angle = 0.001, Q_gyro = 0.005, R_angle = 0.5, C_0 = 1, K1 = 0.05;

// Speed variables
int encoder_left_pulse_num_speed = 0;
int encoder_right_pulse_num_speed = 0;
double speed_control_output = 0;
double rotation_control_output = 0;
double speed_filter = 0;
double speed_filter_old = 0;
int speed_control_period_count = 0;
double car_speed_integeral = 0;

int setting_car_speed = 0;
int setting_turn_speed = 0;

double pwm_left = 0;
double pwm_right = 0;

float kalmanfilter_angle;
char balance_angle_min = -22;
char balance_angle_max = 22;

extern char key_flag;
extern char motion_mode;
extern char key_value;

void carStop() {
  digitalWrite(AIN1, HIGH);
  digitalWrite(BIN1, LOW);
  digitalWrite(STBY_PIN, HIGH);
  analogWrite(PWMA_LEFT, 0);
  analogWrite(PWMB_RIGHT, 0);
}

void carForward(unsigned char speed) {
  digitalWrite(AIN1, LOW);
  digitalWrite(BIN1, LOW);
  analogWrite(PWMA_LEFT, speed);
  analogWrite(PWMB_RIGHT, speed);
}

void carBack(unsigned char speed) {
  digitalWrite(AIN1, HIGH);
  digitalWrite(BIN1, HIGH);
  analogWrite(PWMA_LEFT, speed);
  analogWrite(PWMB_RIGHT, speed);
}

void balanceCar() {
  // *** IMPORTANT : surtout PAS de sei() ici ***

  // Update encoder speed
  encoder_left_pulse_num_speed  += (pwm_left  < 0 ? -encoder_count_left_a  : encoder_count_left_a);
  encoder_right_pulse_num_speed += (pwm_right < 0 ? -encoder_count_right_a : encoder_count_right_a);
  encoder_count_left_a = 0;
  encoder_count_right_a = 0;

  // Read MPU6050
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
  kalmanfilter.Angle(ax, ay, az, gx, gy, gz, dt, Q_angle, Q_gyro, R_angle, C_0, K1);
  kalmanfilter_angle = kalmanfilter.angle;

  // Balance PID
  double balance_control_output =
      kp_balance * (kalmanfilter_angle - angle_zero) +
      kd_balance * (kalmanfilter.Gyro_x - angular_velocity_zero);

  // Speed PID every 8 loops (40ms)
  speed_control_period_count++;
  if (speed_control_period_count >= 8) {
    speed_control_period_count = 0;

    double car_speed = (encoder_left_pulse_num_speed + encoder_right_pulse_num_speed) * 0.5;
    encoder_left_pulse_num_speed = 0;
    encoder_right_pulse_num_speed = 0;

    speed_filter = speed_filter_old * 0.7 + car_speed * 0.3;
    speed_filter_old = speed_filter;

    car_speed_integeral += speed_filter;
    car_speed_integeral -= setting_car_speed;
    car_speed_integeral = constrain(car_speed_integeral, -3000, 3000);

    speed_control_output = -kp_speed * speed_filter - ki_speed * car_speed_integeral;
    rotation_control_output = setting_turn_speed + kd_turn * kalmanfilter.Gyro_z;
  }

  // PWM output
  pwm_left  = balance_control_output - speed_control_output - rotation_control_output;
  pwm_right = balance_control_output - speed_control_output + rotation_control_output;

  pwm_left  = constrain(pwm_left, -255, 255);
  pwm_right = constrain(pwm_right, -255, 255);

  // Fall-over detection
  if (motion_mode != START && motion_mode != STOP &&
      (kalmanfilter_angle < balance_angle_min || kalmanfilter_angle > balance_angle_max)) {
    motion_mode = STOP;
    carStop();
    return;
  }

  // Apply PWM
  if (motion_mode == STOP && key_flag != '4') {
    car_speed_integeral = 0;
    pwm_left = 0;
    pwm_right = 0;
    carStop();
    return;
  }

  // Motor drive
  digitalWrite(AIN1, pwm_left < 0);
  analogWrite(PWMA_LEFT, abs(pwm_left));

  digitalWrite(BIN1, pwm_right < 0);
  analogWrite(PWMB_RIGHT, abs(pwm_right));
}

// Interrupt handlers
void encoderCountRightA() { encoder_count_right_a++; }
void encoderCountLeftA()  { encoder_count_left_a++; }

// Initialization
void carInitialize() {
  pinMode(AIN1, OUTPUT);
  pinMode(BIN1, OUTPUT);
  pinMode(PWMA_LEFT, OUTPUT);
  pinMode(PWMB_RIGHT, OUTPUT);
  pinMode(STBY_PIN, OUTPUT);

  carStop();

  Wire.begin();
  mpu.initialize();

  // ENCODERS: USE D2 & D3
  attachInterrupt(digitalPinToInterrupt(2), encoderCountLeftA, CHANGE);
  attachInterrupt(digitalPinToInterrupt(3), encoderCountRightA, CHANGE);

  MsTimer2::set(5, balanceCar);
  MsTimer2::start();
}
