// ===========================================================================
//  Half-Life Movement - minimal 3-component vector for the pmove port.
//  Kept dependency-free so the mod only references EthhhSDK.Managed.dll.
// ===========================================================================
using System;

namespace HalfLifeMovement
{
    internal struct Vec3
    {
        public float X, Y, Z;

        public Vec3(float x, float y, float z)
        {
            X = x; Y = y; Z = z;
        }

        public static readonly Vec3 Zero = new Vec3(0.0f, 0.0f, 0.0f);

        public float Length => (float)Math.Sqrt(X * X + Y * Y + Z * Z);
        public float LengthSquared => X * X + Y * Y + Z * Z;

        public Vec3 Normalized()
        {
            float len = Length;
            return len > 1e-6f ? this / len : Zero;
        }

        public static float Dot(Vec3 a, Vec3 b) => a.X * b.X + a.Y * b.Y + a.Z * b.Z;

        public static Vec3 Cross(Vec3 a, Vec3 b)
            => new Vec3(a.Y * b.Z - a.Z * b.Y,
                        a.Z * b.X - a.X * b.Z,
                        a.X * b.Y - a.Y * b.X);

        public static Vec3 operator +(Vec3 a, Vec3 b) => new Vec3(a.X + b.X, a.Y + b.Y, a.Z + b.Z);
        public static Vec3 operator -(Vec3 a, Vec3 b) => new Vec3(a.X - b.X, a.Y - b.Y, a.Z - b.Z);
        public static Vec3 operator -(Vec3 a) => new Vec3(-a.X, -a.Y, -a.Z);
        public static Vec3 operator *(Vec3 a, float s) => new Vec3(a.X * s, a.Y * s, a.Z * s);
        public static Vec3 operator *(float s, Vec3 a) => a * s;
        public static Vec3 operator /(Vec3 a, float s) => new Vec3(a.X / s, a.Y / s, a.Z / s);

        public override string ToString() => string.Format("{0:F2}, {1:F2}, {2:F2}", X, Y, Z);
    }
}
