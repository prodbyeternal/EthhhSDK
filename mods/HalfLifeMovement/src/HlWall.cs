// ===========================================================================
//  Half-Life Movement - I-Ninja wall interaction state.
//
//  I-Ninja's traversal is wall-first: a jump at a wall should start a climb or
//  wall run (Space again kicks off), not require the Half-Life jump key. This
//  class keeps that state independent from the HL pmove port so it can be
//  exercised by the offline tests without a live game.
//
//  The caller supplies an independent wall probe contact and a wish direction.
//  This class never traces and never writes the player; it only produces the
//  velocity the caller should sweep through HlCollision.
// ===========================================================================
using System;

namespace HalfLifeMovement
{
    internal enum WallMode { None, Climb, Run }

    internal enum WallTickResult { Inactive, Active, Detached }

    internal sealed class HlWall
    {
        internal const float RunMinSpeed = 180.0f;
        internal const float ClimbSpeed = 210.0f;
        internal const float ClimbAccel = 1000.0f;
        internal const float RunInitialUp = 130.0f;
        internal const float RunGravity = 500.0f;
        internal const float SlideSpeed = 90.0f;
        internal const float KickUp = 280.0f;
        internal const float KickOut = 200.0f;
        internal const float StickSpeed = 48.0f;
        internal const int ClimbTicks = 42;
        internal const int RunTicks = 48;
        internal const int KickLockTicks = 10;
        internal const int LostTolerance = 2;
        internal const int ClimbGraceTicks = 12;
        // Same boundary HlCollision uses for "not a walkable floor".
        internal const float WallNormalY = HlCollision.WalkableNormalY;

        public WallMode Mode { get; private set; }
        public Vec3 Normal { get; private set; }
        public Vec3 Tangent { get; private set; }
        public float VerticalSpeed { get; private set; }
        public float TangentSpeed { get; private set; }
        public int TicksRemaining { get; private set; }
        public int KickLock { get; private set; }
        public bool Active => Mode != WallMode.None;
        private int _lostTicks;

        public void Reset()
        {
            Mode = WallMode.None;
            Normal = Vec3.Zero;
            Tangent = Vec3.Zero;
            VerticalSpeed = TangentSpeed = 0.0f;
            TicksRemaining = _lostTicks = 0;
            KickLock = 0;
        }

        public void TickLock()
        {
            if (KickLock > 0) KickLock--;
        }

        public static bool WallLike(Vec3 normal)
        {
            return Finite(normal) && normal.LengthSquared > 0.25f && normal.LengthSquared < 2.25f &&
                   normal.Y >= 0.0f && normal.Y < WallNormalY;
        }

        public bool TryAttach(Vec3 normal, Vec3 viewDir, Vec3 velocity, HlTuning tune)
        {
            if (KickLock > 0 || !WallLike(normal) || tune == null) return false;
            Vec3 n = normal.Normalized();
            Vec3 tangent = ChooseTangent(n, viewDir, velocity);
            if (tangent.LengthSquared < 0.25f) return false;

            Mode = WallMode.None;
            Normal = n;
            Tangent = tangent;
            _lostTicks = 0;

            float horizontalSpeed = new Vec3(velocity.X, 0.0f, velocity.Z).Length;
            Mode = horizontalSpeed >= RunMinSpeed ? WallMode.Run : WallMode.Climb;
            TicksRemaining = Mode == WallMode.Run ? RunTicks : ClimbTicks;
            VerticalSpeed = Mode == WallMode.Run ? RunInitialUp : 0.0f;
            TangentSpeed = Mode == WallMode.Run ? Math.Max(horizontalSpeed, tune.MaxSpeed * 0.9f) : 0.0f;
            return true;
        }

        // contact is the independent wall probe result for this tick. hold keeps
        // a climb going (Space or forward held); a short grace still lets the
        // first jump attach even if the key is released immediately.
        public WallTickResult Tick(float dt, bool contact, bool hold, Vec3 wishDir, float maxSpeed,
                                   out Vec3 velocity)
        {
            velocity = Vec3.Zero;
            if (Mode == WallMode.None) return WallTickResult.Inactive;
            if (!Finite(dt) || dt <= 0.0f || !Finite(wishDir) || !Finite(maxSpeed) || maxSpeed <= 0.0f)
                return Detach(out velocity);

            if (!contact) {
                if (++_lostTicks > LostTolerance) return Detach(out velocity);
            } else {
                _lostTicks = 0;
            }
            if (TicksRemaining <= 0) return Detach(out velocity);
            TicksRemaining--;

            if (Mode == WallMode.Climb) {
                bool climbing = hold || TicksRemaining > ClimbTicks - ClimbGraceTicks;
                float target = climbing ? ClimbSpeed : -SlideSpeed;
                float accel = climbing ? ClimbAccel : RunGravity;
                VerticalSpeed = MoveTowards(VerticalSpeed, target, accel * dt);
                float along = Clamp(Vec3.Dot(wishDir, Tangent), -1.0f, 1.0f);
                TangentSpeed = MoveTowards(TangentSpeed, along * maxSpeed * 0.5f, maxSpeed * 4.0f * dt);
            } else {
                VerticalSpeed = Math.Max(-SlideSpeed, VerticalSpeed - RunGravity * dt);
                float along = Vec3.Dot(wishDir, Tangent);
                float sign = Math.Abs(along) > 0.2f ? Math.Sign(along) : Math.Sign(TangentSpeed);
                float magnitude = Math.Max(Math.Abs(TangentSpeed), maxSpeed * 0.8f);
                TangentSpeed = MoveTowards(TangentSpeed, sign * magnitude, maxSpeed * 3.0f * dt);
            }

            Vec3 horizontalNormal = new Vec3(Normal.X, 0.0f, Normal.Z).Normalized();
            if (horizontalNormal.LengthSquared < 0.25f) return Detach(out velocity);
            velocity = Tangent * TangentSpeed + new Vec3(0.0f, VerticalSpeed, 0.0f) -
                       horizontalNormal * StickSpeed;
            return WallTickResult.Active;
        }

        // Space while attached kicks away from the wall. The caller turns the
        // vertical part into its normal gravity/landing integration.
        public Vec3 Kick(out float verticalSpeed)
        {
            verticalSpeed = KickUp;
            Vec3 horizontalNormal = new Vec3(Normal.X, 0.0f, Normal.Z);
            Vec3 away = horizontalNormal.LengthSquared > 1e-6f
                ? horizontalNormal.Normalized()
                : new Vec3(0.0f, 0.0f, 1.0f);
            Vec3 horizontal = away * KickOut + Tangent * (TangentSpeed * 0.6f);

            Mode = WallMode.None;
            Normal = Vec3.Zero;
            Tangent = Vec3.Zero;
            VerticalSpeed = TangentSpeed = 0.0f;
            TicksRemaining = _lostTicks = 0;
            KickLock = KickLockTicks;
            return horizontal;
        }

        private WallTickResult Detach(out Vec3 velocity)
        {
            velocity = Tangent * TangentSpeed + new Vec3(0.0f, VerticalSpeed, 0.0f);
            Mode = WallMode.None;
            Normal = Vec3.Zero;
            Tangent = Vec3.Zero;
            VerticalSpeed = TangentSpeed = 0.0f;
            TicksRemaining = _lostTicks = 0;
            return WallTickResult.Detached;
        }

        // The horizontal line where the wall plane meets the ground. Pick the
        // sign from current motion first, then from the view, so a wall run
        // continues the direction the player was already moving.
        private static Vec3 ChooseTangent(Vec3 normal, Vec3 viewDir, Vec3 velocity)
        {
            Vec3 tangent = Vec3.Cross(new Vec3(0.0f, 1.0f, 0.0f), normal);
            if (tangent.LengthSquared < 1e-4f) return Vec3.Zero;
            tangent = tangent.Normalized();

            Vec3 horizontalVelocity = new Vec3(velocity.X, 0.0f, velocity.Z);
            Vec3 horizontalView = new Vec3(viewDir.X, 0.0f, viewDir.Z);
            float sign = 1.0f;
            if (horizontalVelocity.LengthSquared > 1e-4f &&
                Math.Abs(Vec3.Dot(tangent, horizontalVelocity)) > 0.1f)
                sign = Vec3.Dot(tangent, horizontalVelocity) >= 0.0f ? 1.0f : -1.0f;
            else if (horizontalView.LengthSquared > 1e-4f)
                sign = Vec3.Dot(tangent, horizontalView) >= 0.0f ? 1.0f : -1.0f;
            return tangent * sign;
        }

        private static float MoveTowards(float value, float target, float maxDelta)
        {
            if (!Finite(value) || !Finite(target) || !Finite(maxDelta) || maxDelta <= 0.0f) return value;
            float delta = target - value;
            return Math.Abs(delta) <= maxDelta ? target : value + Math.Sign(delta) * maxDelta;
        }

        private static float Clamp(float value, float lo, float hi)
            => float.IsNaN(value) ? 0.0f : (value < lo ? lo : (value > hi ? hi : value));

        private static bool Finite(Vec3 v) => Finite(v.X) && Finite(v.Y) && Finite(v.Z);
        private static bool Finite(float v) => !float.IsNaN(v) && !float.IsInfinity(v);
    }
}
