/* Copyright (C) 2018-2019 Thomas Jespersen, TKJ Electronics. All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details. 
 *
 * Contact information
 * ------------------------------------------
 * Thomas Jespersen, TKJ Electronics
 * Web      :  http://www.tkjelectronics.dk
 * e-mail   :  thomasj@tkjelectronics.dk
 * ------------------------------------------
 */
 
#include "QuaternionVelocityControl.h"
 
#include <arm_math.h>
#include <math.h>
#include <stdlib.h>
#include <cmath>

#include "Quaternion.h"
#include "Parameters.h"

QuaternionVelocityControl::QuaternionVelocityControl(Parameters& params, Timer * microsTimer, float SamplePeriod, float ReferenceSmoothingTau) : _params(params), _microsTimer(microsTimer), _dx_ref_filt(SamplePeriod, ReferenceSmoothingTau), _dy_ref_filt(SamplePeriod, ReferenceSmoothingTau)
{
	Reset();
}

QuaternionVelocityControl::QuaternionVelocityControl(Parameters& params, float SamplePeriod, float ReferenceSmoothingTau) : _params(params), _microsTimer(0), _dx_ref_filt(SamplePeriod, ReferenceSmoothingTau), _dy_ref_filt(SamplePeriod, ReferenceSmoothingTau)
{
	Reset();
}

QuaternionVelocityControl::~QuaternionVelocityControl()
{
}

void QuaternionVelocityControl::Reset()
{
	if (_microsTimer)
		_prevTimerValue = _microsTimer->Get();
	else
		_prevTimerValue = 0;

	// Reset integral quaternion to the unit quaternion (no integral)
	q_tilt_integral[0] = 1;
	q_tilt_integral[1] = 0;
	q_tilt_integral[2] = 0;
	q_tilt_integral[3] = 0;
}

void QuaternionVelocityControl::Step(const float q[4], const float dq[4], const float dxy[2], const float velocityRef[2], const bool velocityRefGivenInHeadingFrame, const float headingRef, float q_ref_out[4])
{
	float dt;

	if (!_microsTimer) return; // timer not defined
	dt = _microsTimer->GetDeltaTime(_prevTimerValue);
	_prevTimerValue = _microsTimer->Get();

	Step(q, dq, dxy, velocityRef, velocityRefGivenInHeadingFrame, headingRef, dt, q_ref_out);
}

void QuaternionVelocityControl::Step(const float q[4], const float dq[4], const float dxy[2], const float velocityRef[2], const bool velocityRefGivenInHeadingFrame, const float headingRef, const float dt, float q_ref_out[4])
{
	//const float Velocity_Inertial_q[4] = {0, dx_filt.sample(fmin(fmax(*dx, -CLAMP_VELOCITY), CLAMP_VELOCITY)), dy_filt.sample(fmin(fmax(*dy, -CLAMP_VELOCITY), CLAMP_VELOCITY)), 0};
	float Velocity_Inertial_q[4] = {0, dxy[0], dxy[1], 0};
	float Velocity_Heading_q[4];
	float Velocity_Heading[2];

	float Velocity_Reference_Filtered[2];
	Velocity_Reference_Filtered[0] = _dx_ref_filt.Filter(velocityRef[0]);
	Velocity_Reference_Filtered[1] = _dy_ref_filt.Filter(velocityRef[1]);

	if (!velocityRefGivenInHeadingFrame) { // reference given in inertial frame
		// Calculate velocity error
		Velocity_Inertial_q[1] -= Velocity_Reference_Filtered[0];
		Velocity_Inertial_q[2] -= Velocity_Reference_Filtered[1];
	}

	//Velocity_Heading = [0,1,0,0;0,0,1,0] * Phi(q)' * Gamma(q) * [0;Velocity_Inertial;0];
	float tmp_q[4];
	Quaternion_Gamma(q, Velocity_Inertial_q, tmp_q); // Gamma(q) * [0;Velocity_Inertial;0];
	Quaternion_PhiT(q, tmp_q, Velocity_Heading_q); // Phi(q)' * Gamma(q) * [0;Velocity_Inertial;0];
	Velocity_Heading[0] = Velocity_Heading_q[1];
	Velocity_Heading[1] = Velocity_Heading_q[2];
	// OBS. The above could also be replaced with a Quaternion transformation function that takes in a 3-dimensional vector

	if (velocityRefGivenInHeadingFrame) { // reference given in heading frame
		// Calculate velocity error
		Velocity_Heading[0] -= Velocity_Reference_Filtered[0];
		Velocity_Heading[1] -= Velocity_Reference_Filtered[1];
	}

	Velocity_Heading[0] = fmin(fmax(Velocity_Heading[0], -_params.controller.VelocityController_VelocityClamp), _params.controller.VelocityController_VelocityClamp);
	Velocity_Heading[1] = fmin(fmax(Velocity_Heading[1], -_params.controller.VelocityController_VelocityClamp), _params.controller.VelocityController_VelocityClamp);

	/*Velocity_Heading_Integral[0] += INTEGRAL_GAIN/_SampleRate * fmin(fmax(Velocity_Heading[0], -CLAMP_VELOCITY), CLAMP_VELOCITY); // INTEGRAL_GAIN * dt * velocity
	Velocity_Heading_Integral[1] += INTEGRAL_GAIN/_SampleRate * fmin(fmax(Velocity_Heading[1], -CLAMP_VELOCITY), CLAMP_VELOCITY);

	Velocity_Heading[0] += Velocity_Heading_Integral[0]; // correct with integral error
	Velocity_Heading[1] += Velocity_Heading_Integral[1];*/

	float q_heading[4]; // heading reference quaternion based on heading reference
	q_heading[0] = cosf(headingRef/2);
	q_heading[1] = 0;
	q_heading[2] = 0;
	q_heading[3] = sinf(headingRef/2);

	float normVelocity_Heading = sqrtf(Velocity_Heading[0]*Velocity_Heading[0] + Velocity_Heading[1]*Velocity_Heading[1]);
	if (normVelocity_Heading == 0) {
		// Return upright quaternion (with heading reference) if we have no velocity, since we are then where we are supposed to be
		q_ref_out[0] = q_heading[0];
		q_ref_out[1] = q_heading[1];
		q_ref_out[2] = q_heading[2];
		q_ref_out[3] = q_heading[3];
		return;
	}

	//CorrectionDirection = [0, 1; -1, 0] * Velocity_Heading / normVelocity_Heading;
	float CorrectionDirection[2] = {Velocity_Heading[1] / normVelocity_Heading,
																 -Velocity_Heading[0] / normVelocity_Heading};

	// CorrectionAmountRadian = min(max((normVelocity_Heading / 5), -1), 1) * deg2rad(30); % max 5 m/s resulting in 30 degree tilt <= this is the proportional gain
	float CorrectionAmountRadian = fmin(fmax(normVelocity_Heading / _params.controller.VelocityController_VelocityClamp, -1.0f), 1.0f) * deg2rad(_params.controller.VelocityController_MaxTilt);

	float q_tilt[4]; // tilt reference quaternion defined in heading frame
	q_tilt[0] = cosf(CorrectionAmountRadian/2);
	q_tilt[1] = sinf(CorrectionAmountRadian/2)*CorrectionDirection[0];
	q_tilt[2] = sinf(CorrectionAmountRadian/2)*CorrectionDirection[1];
	q_tilt[3] = 0;

	if (velocityRef[0] != 0 || velocityRef[1] != 0) {
		CorrectionAmountRadian = 0; // only do integral action when velocity reference is zero
	}

	float q_tilt_integral_incremental[4];
	q_tilt_integral_incremental[0] = cosf(CorrectionAmountRadian/2 * _params.controller.VelocityController_IntegralGain * dt);
	q_tilt_integral_incremental[1] = sinf(CorrectionAmountRadian/2 * _params.controller.VelocityController_IntegralGain * dt)*CorrectionDirection[0];
	q_tilt_integral_incremental[2] = sinf(CorrectionAmountRadian/2 * _params.controller.VelocityController_IntegralGain * dt)*CorrectionDirection[1];
	q_tilt_integral_incremental[3] = 0;

	float q_tilt_integral_old[4];
	for (int i = 0; i < 4; i++) q_tilt_integral_old[i] = q_tilt_integral[i];

	Quaternion_Phi(q_tilt_integral_old, q_tilt_integral_incremental, q_tilt_integral);
	Quaternion_AngleClamp(q_tilt_integral, deg2rad(_params.controller.VelocityController_MaxIntegralCorrection), q_tilt_integral);

	float q_tilt_withintegral[4];
	Quaternion_Phi(q_tilt, q_tilt_integral, q_tilt_withintegral);

	// Add heading to quaternion reference = q_heading o q_tilt
	Quaternion_Phi(q_heading, q_tilt_withintegral, q_ref_out);

	/*Serial.printf("%2.5f, %2.5f, ", Velocity_Heading[0], Velocity_Heading[1]);
	Serial.printf("%2.5f, %2.5f, %2.5f, %2.5f, ", q_tilt_integral[0], q_tilt_integral[1], q_tilt_integral[2], q_tilt_integral[3]);
	Serial.printf("%2.5f, %2.5f, %2.5f, %2.5f\n", q_ref_out[0], q_ref_out[1], q_ref_out[2], q_ref_out[3]);*/
}
