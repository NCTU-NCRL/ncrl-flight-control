#include <math.h>
#include <stdio.h>
#include "arm_math.h"
#include "ahrs.h"
#include "comp_ahrs.h"
#include "lpf.h"
#include "matrix.h"
#include "se3_math.h"
#include "quaternion.h"

/* check the paper: Keeping a Good Attitude: A Quaternion-Based Orientation Filter for IMUs and MA
 * by Roberto G. Valenti, Ivan Dryanovski and Jizhong Xiao */

MAT_ALLOC(q, 4, 1);
MAT_ALLOC(R_gyro, 3, 3);
MAT_ALLOC(accel_vec, 3, 1);
MAT_ALLOC(g_predict, 3, 1);

float comp_ahrs_dt = 0.0f;

void complementary_ahrs_init(float ahrs_dt)
{
	MAT_INIT(q, 4, 1);
	MAT_INIT(R_gyro, 3, 3);
	MAT_INIT(accel_vec, 3, 1);
	MAT_INIT(g_predict, 3, 1);

	comp_ahrs_dt = ahrs_dt;

	_mat_(q)[0] = 1.0f;
	_mat_(q)[1] = 0.0f;
	_mat_(q)[2] = 0.0f;
	_mat_(q)[3] = 0.0f;
}

void convert_gravity_to_quat(float *a, float *q)
{
	float _sqrt;

	if(a[2] >= 0.0f) {
		//q0
		arm_sqrt_f32(0.5f * (a[2] + 1.0f), &_sqrt);
		q[0] = _sqrt;
		//q1
		arm_sqrt_f32(2.0f * (a[2] + 1.0f), &_sqrt);
		q[1] = -a[1] / _sqrt;
		//q2
		arm_sqrt_f32(2.0f * (a[2] + 1.0f), &_sqrt);
		q[2] = +a[0] / _sqrt;
		//q3
		q[3] = 0.0f;
	} else {
		//q0
		arm_sqrt_f32(2.0f * (1.0f - a[2]), &_sqrt);
		q[0] = -a[1] / _sqrt;
		//q1
		arm_sqrt_f32((1.0f - a[2]) * 0.5f, &_sqrt);
		q[1] = _sqrt;
		//q2
		q[2] = 0.0f;
		//q3
		arm_sqrt_f32(2.0f * (1.0f - a[2]), &_sqrt);
		q[3] = a[0] / _sqrt;
	}

	quat_normalize(q);
}

void convert_magnetic_field_to_quat(float *l, float *q)
{
	float gamma = l[0]*l[0] + l[1]*l[1];
	float sqrt_gamma, sqrt_2gamma, sqrt_2;
	arm_sqrt_f32(gamma, &sqrt_gamma);
	arm_sqrt_f32(2.0f * gamma, &sqrt_2gamma);
	arm_sqrt_f32(2.0f, &sqrt_2); //TODO: this is constant!

	float _sqrt;

	if(l[0] >= 0.0f) {
		arm_sqrt_f32(gamma + l[0]*sqrt_gamma, &_sqrt);
		q[0] = _sqrt / sqrt_2gamma;
		q[1] = 0.0f;
		q[2] = 0.0f;
		q[3] = l[1] / (sqrt_2 * _sqrt);
	} else {
		arm_sqrt_f32(gamma - l[0]*sqrt_gamma, &_sqrt);
		q[0] = l[1] / (sqrt_2 * _sqrt);
		q[1] = 0.0f;
		q[2] = 0.0f;
		q[3] = _sqrt / sqrt_2gamma;
	}

	quat_normalize(q);
}

void prepare_body_to_earth_rotation_matrix(float *q, float *r)
{
	float q0q0 = q[0] * q[0];
	float q1q1 = q[1] * q[1];
	float q2q2 = q[2] * q[2];
	float q3q3 = q[3] * q[3];
	float q0q3 = q[0] * q[3];
	float q1q2 = q[1] * q[2];
	float q2q3 = q[2] * q[3];
	float q1q3 = q[1] * q[3];
	float q0q2 = q[0] * q[2];
	float q0q1 = q[0] * q[1];

	r[0*3 + 0] = q0q0 + q1q1 - q2q2 - q3q3;
	r[0*3 + 1] = 2*(q1q2 - q0q3);
	r[0*3 + 2] = 2*(q1q3 + q0q2);

	r[1*3 + 0] = 2*(q1q2 + q0q3);
	r[1*3 + 1] = q0q0 - q1q1 + q2q2 - q3q3;
	r[1*3 + 2] = 2*(q2q3 - q0q1);

	r[2*3 + 0] = 2*(q1q3 - q0q2);
	r[2*3 + 1] = 2*(q2q3 + q0q1);
	r[2*3 + 2] = q0q0 - q1q1 - q2q2 + q3q3;
}

void lerp(float *q1, float *q2, float alpha, float *q_out)
{
	q_out[0] = ((1.0f - alpha) * q1[0]) + (alpha * q2[0]);
	q_out[1] = ((1.0f - alpha) * q1[1]) + (alpha * q2[1]);
	q_out[2] = ((1.0f - alpha) * q1[2]) + (alpha * q2[2]);
	q_out[3] = ((1.0f - alpha) * q1[3]) + (alpha * q2[3]);
}

void ahrs_imu_complementary_filter_estimate(float *q_out, float *accel, float *gyro)
{
	/* quaternion integration (with gyroscope) */
	float w[4];
	w[0] = 0.0f;
	w[1] = gyro[0];
	w[2] = gyro[1];
	w[3] = gyro[2];

	float q_dot[4];
	quaternion_mult(w, _mat_(q), q_dot);

	float q_gyro[4];
	float half_dt = -0.5 * comp_ahrs_dt;
	q_gyro[0] = _mat_(q)[0] + (q_dot[0] * half_dt);
	q_gyro[1] = _mat_(q)[1] + (q_dot[1] * half_dt);
	q_gyro[2] = _mat_(q)[2] + (q_dot[2] * half_dt);
	q_gyro[3] = _mat_(q)[3] + (q_dot[3] * half_dt);
	quat_normalize(q_gyro);

	/* calculate predicted gravity vector */
	float conj_q_gyro[4];
	quaternion_conj(q_gyro, conj_q_gyro);
	prepare_body_to_earth_rotation_matrix(conj_q_gyro, _mat_(R_gyro));

	normalize_3x1(accel); //normalize acceleromter
	_mat_(accel_vec)[0] = accel[0];
	_mat_(accel_vec)[1] = accel[1];
	_mat_(accel_vec)[2] = accel[2];
	MAT_MULT(&R_gyro, &accel_vec, &g_predict);

	/* fuse gyroscope and accelerometer with LERP */
	float delta_q_acc[4];
	convert_gravity_to_quat(_mat_(g_predict), delta_q_acc);
	quat_normalize(delta_q_acc);

	float q_identity[4] = {1.0f, 0.0f, 0.0f, 0.0f};
	float bar_delta_q_acc[4];
	float weight_accel = 0.005f; //alpha value for fusion
	lerp(q_identity, delta_q_acc, weight_accel, bar_delta_q_acc);
	quat_normalize(bar_delta_q_acc);
	quaternion_mult(q_gyro, bar_delta_q_acc, _mat_(q));

	/* return the conjugated quaternion since we use opposite convention compared to the paper.
	 * paper: quaternion of earth frame to body-fixed frame
	 * us: quaternion of body-fixed frame to earth frame */
	quaternion_conj(_mat_(q), q_out);
}

void ahrs_marg_complementary_filter_estimate(float *q_out, float *accel, float *gyro, float *mag)
{
}
