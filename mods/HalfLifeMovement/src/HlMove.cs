// ===========================================================================
//  Half-Life Movement - pmove port.
//
//  Re-implementation of the acceleration/friction math from Valve's HLSDK
//  (halflife-steam_legacy/pm_shared/pm_shared.c):
//
//      PM_Friction          -> ApplyFriction
//      PM_Accelerate        -> Accelerate
//      PM_AirAccelerate     -> AirAccelerate
//      PM_WalkMove          -> WalkMove
//      PM_AirMove           -> AirMove
//      PM_PreventMegaBunnyJumping -> CapBunnySpeed
//      PM_CheckVelocity     -> ClampVelocity
//
//  Collision (PM_FlyMove / PM_ClipVelocity) is intentionally *not* ported:
//  no verified I-Ninja hull-trace adapter is available yet. Post-tick position
//  writes are experimental and may bypass the game's collision corrections.
//  This mod supplies the HL acceleration/friction/gravity/jump model and
//  writes the resulting displacement after the game's scripts have run.
//
//  Half-Life defaults (single-player): gravity 800, maxspeed 320,
//  accelerate 10, airaccelerate 10, friction 4, stopspeed 100.
// ===========================================================================
using System;

namespace HalfLifeMovement
{
    /// <summary>Movement variables, equivalent to HLSDK movevars_t + player friction.</summary>
    internal sealed class HlTuning
    {
        public float Gravity = 800.0f;
        public float MaxSpeed = 320.0f;
        public float Accelerate = 10.0f;
        public float AirAccelerate = 10.0f;
        public float Friction = 4.0f;
        public float StopSpeed = 100.0f;
        public float MaxVelocity = 2000.0f;
        public float JumpSpeed = 268.328f;   // sqrt(2 * 800 * 45), HLSDK PM_Jump
        public float BunnyMaxSpeedFactor = 1.7f;
        public float AirWishSpeedCap = 30.0f;

        /// <summary>
        /// Vertical speed (units/second) the *game* must produce before we stop
        /// treating the player as standing on ground (stairs are below this,
        /// jumps and falls are above it).
        /// </summary>
        public float GameVerticalThreshold = 180.0f;
    }

    /// <summary>Player velocity state and the HLSDK acceleration model.</summary>
    internal sealed class HlMove
    {
        public Vec3 Velocity;

        public float Speed => Velocity.Length;

        public void Reset() => Velocity = Vec3.Zero;

        // ------------------------------------------------------------- friction
        /// <summary>HLSDK PM_Friction, ground branch. Player friction multiplier is 1.</summary>
        public void ApplyFriction(float dt, HlTuning t)
        {
            float speed = Velocity.Length;
            if (speed < 0.1f)
                return;

            float drop = 0.0f;
            // Ground friction only (this port only runs friction while grounded).
            float control = speed < t.StopSpeed ? t.StopSpeed : speed;
            drop += control * t.Friction * dt;

            float newspeed = speed - drop;
            if (newspeed < 0.0f) newspeed = 0.0f;
            newspeed /= speed;

            Velocity = Velocity * newspeed;
        }

        // ----------------------------------------------------------- accelerate
        /// <summary>HLSDK PM_Accelerate.</summary>
        public void Accelerate(Vec3 wishDir, float wishSpeed, float accel, float dt)
        {
            float currentSpeed = Vec3.Dot(Velocity, wishDir);
            float addSpeed = wishSpeed - currentSpeed;
            if (addSpeed <= 0.0f)
                return;

            float accelSpeed = accel * dt * wishSpeed;   // * pmove->friction (1.0)
            if (accelSpeed > addSpeed)
                accelSpeed = addSpeed;

            Velocity += wishDir * accelSpeed;
        }

        /// <summary>HLSDK PM_AirAccelerate (wish speed capped at 30 u/s).</summary>
        public void AirAccelerate(Vec3 wishDir, float wishSpeed, float accel, float dt, HlTuning t)
        {
            float wishSpd = wishSpeed;
            if (wishSpd > t.AirWishSpeedCap)
                wishSpd = t.AirWishSpeedCap;

            float currentSpeed = Vec3.Dot(Velocity, wishDir);
            float addSpeed = wishSpd - currentSpeed;
            if (addSpeed <= 0.0f)
                return;

            float accelSpeed = accel * wishSpeed * dt;   // * pmove->friction (1.0)
            if (accelSpeed > addSpeed)
                accelSpeed = addSpeed;

            Velocity += wishDir * accelSpeed;
        }

        // ---------------------------------------------------------- walk / air
        /// <summary>HLSDK PM_WalkMove (ground acceleration; z is zeroed).</summary>
        public void WalkMove(Vec3 wishDir, float wishSpeed, float dt, HlTuning t)
        {
            Velocity.Y = 0.0f;
            Accelerate(wishDir, wishSpeed, t.Accelerate, dt);
            Velocity.Y = 0.0f;

            if (Velocity.Length < 1.0f)
                Velocity = Vec3.Zero;
        }

        /// <summary>HLSDK PM_AirMove (air acceleration; no world trace).</summary>
        public void AirMove(Vec3 wishDir, float wishSpeed, float dt, HlTuning t)
        {
            AirAccelerate(wishDir, wishSpeed, t.AirAccelerate, dt, t);
        }

        // -------------------------------------------------------------- helpers
        /// <summary>HLSDK PM_PreventMegaBunnyJumping (called by PM_Jump).</summary>
        public void CapBunnySpeed(HlTuning t)
        {
            float maxScaled = t.BunnyMaxSpeedFactor * t.MaxSpeed;
            if (maxScaled <= 0.0f)
                return;

            float speed = Velocity.Length;
            if (speed <= maxScaled)
                return;

            float fraction = (maxScaled / speed) * 0.65f;
            Velocity = Velocity * fraction;
        }

        /// <summary>HLSDK PM_CheckVelocity bounds every component to +/- maxvelocity.</summary>
        public void ClampVelocity(HlTuning t)
        {
            Velocity.X = Clamp(Velocity.X, -t.MaxVelocity, t.MaxVelocity);
            Velocity.Y = Clamp(Velocity.Y, -t.MaxVelocity, t.MaxVelocity);
            Velocity.Z = Clamp(Velocity.Z, -t.MaxVelocity, t.MaxVelocity);
        }

        private static float Clamp(float v, float lo, float hi)
            => float.IsNaN(v) ? 0.0f : (v < lo ? lo : (v > hi ? hi : v));
    }

    internal static class HlJump
    {
        // HLSDK half-gravity before and after movement. Height is an offset
        // from takeoff, never an offset added to last tick's written position.
        public static void Step(ref float height, ref float velocity, float gravity, float dt)
        {
            velocity -= gravity * dt * 0.5f;
            height += velocity * dt;
            velocity -= gravity * dt * 0.5f;
            if (height <= 0.0f)
            {
                height = 0.0f;
                velocity = 0.0f;
            }
        }
    }

    /// <summary>
    /// Builds a row-major, row-vector camera world matrix identical in layout to
    /// the game's camera (+0x80): rows 0..2 = right/up/forward, row 3 = position.
    /// Matches eth::game::MakeCameraWorld so the engine's Fn_UpdateView accepts it.
    /// </summary>
    internal static class HlCamera
    {
        public static void MakeWorld(Vec3 position, float yawRad, float pitchRad, float rollRad, float[] m)
        {
            if (m == null || m.Length < 16)
                throw new ArgumentException("camera matrix must hold 16 floats", nameof(m));

            float cy = (float)Math.Cos(yawRad), sy = (float)Math.Sin(yawRad);
            float cp = (float)Math.Cos(pitchRad), sp = (float)Math.Sin(pitchRad);

            // +Z is forward at yaw 0, +Y up, +X right (left-handed, D3D style).
            float fx = sy * cp;
            float fy = sp;
            float fz = cy * cp;

            // right = normalize(cross(worldUp, forward))
            float rx = fz, ry = 0.0f, rz = -fx;
            float rl = (float)Math.Sqrt(rx * rx + ry * ry + rz * rz);
            if (rl < 1e-6f) { rx = 1.0f; ry = 0.0f; rz = 0.0f; rl = 1.0f; }
            rx /= rl; ry /= rl; rz /= rl;

            // up = cross(forward, right)
            float ux = fy * rz - fz * ry;
            float uy = fz * rx - fx * rz;
            float uz = fx * ry - fy * rx;

            if (rollRad != 0.0f)
            {
                float cr = (float)Math.Cos(rollRad), sr = (float)Math.Sin(rollRad);
                float nrx = rx * cr + ux * sr;
                float nry = ry * cr + uy * sr;
                float nrz = rz * cr + uz * sr;
                float nux = ux * cr - rx * sr;
                float nuy = uy * cr - ry * sr;
                float nuz = uz * cr - rz * sr;
                rx = nrx; ry = nry; rz = nrz;
                ux = nux; uy = nuy; uz = nuz;
            }

            m[0] = rx; m[1] = ry; m[2] = rz; m[3] = 0.0f;
            m[4] = ux; m[5] = uy; m[6] = uz; m[7] = 0.0f;
            m[8] = fx; m[9] = fy; m[10] = fz; m[11] = 0.0f;
            m[12] = position.X; m[13] = position.Y; m[14] = position.Z; m[15] = 1.0f;
        }
    }
}
