// Experimental segment-bundle adapter, NOT a native hull or start-solid query.
// It never moves a player: the caller commits only after every query succeeds.
using System;
using EthhhSDK;

namespace HalfLifeMovement
{
    internal sealed class HlCollision
    {
        internal delegate bool TraceBatch(uint player, TraceSegment[] segments, TraceHit[] hits);
        private readonly TraceBatch _trace;
        private readonly TraceSegment[] _support = new TraceSegment[5], _body = new TraceSegment[9], _vertical = new TraceSegment[5];
        private readonly TraceHit[] _supportHits = new TraceHit[5], _bodyHits = new TraceHit[9], _verticalHits = new TraceHit[5];
        private readonly TraceSegment[] _wall = new TraceSegment[16], _step = new TraceSegment[5];
        private readonly TraceHit[] _wallHits = new TraceHit[16], _stepHits = new TraceHit[5];
        private readonly Vec3[] _planes = new Vec3[3];
        private static readonly float[] OffsetX = { 0, -1, 1, 0, 0 }, OffsetZ = { 0, 0, 0, -1, 1 };
        private static readonly float[] BodyHeight = { 4, 36, 72 };
        internal const float Radius = 12, Height = 72, Skin = 0.5f, GroundReach = 2;
        // Surface classification. Walkable floors and the lower half-pipe use
        // normal support; steeper surfaces are traversed by step support or the
        // wall run/climb state instead of acting like invisible vertical walls.
        internal const float WalkableNormalY = 0.7f, SteepNormalY = 0.5f;
        internal const float StepHeight = 18.0f;
        // Wall probe: eight horizontal directions at two body heights. The batch
        // is exactly 16 traces, the verified EthGame_TraceSegments limit.
        internal const float WallReach = 2.0f, WallProbeLow = 24.0f, WallProbeHigh = 54.0f;
        internal const float WallFacingMin = 0.15f;

        public HlCollision(TraceBatch trace) { _trace = trace; }

        public bool TryGround(uint player, Vec3 position, float scale, out bool supported, out float groundY, out Vec3 normal)
        {
            supported = false; groundY = float.NegativeInfinity; normal = new Vec3(0, 1, 0);
            if (!Valid(position, scale)) return false;
            for (int i = 0; i < 5; i++) {
                float x = position.X + OffsetX[i] * Radius * scale, z = position.Z + OffsetZ[i] * Radius * scale;
                _support[i] = new TraceSegment(x, position.Y + GroundReach * scale, z, x, position.Y - GroundReach * scale, z);
            }
            if (!_trace(player, _support, _supportHits)) return false;
            for (int i = 0; i < 5; i++) {
                if (!_supportHits[i].HasHit) continue;
                if (!Contact(_support[i], _supportHits[i], scale, out Vec3 n)) return false;
                // Report the highest steep/walkable surface so callers can
                // distinguish a slope from a wall, but only walkable normals
                // grant support/jump permission here. Step support handles the
                // steep half-pipe transition separately.
                if (n.Y >= SteepNormalY && _supportHits[i].PointY > groundY) { groundY = _supportHits[i].PointY; normal = n; }
            }
            float gap = position.Y - groundY;
            supported = Finite(groundY) && normal.Y >= WalkableNormalY &&
                        gap <= GroundReach * scale && gap >= -Skin * scale;
            return true; // No hit is a successful query, not permission to invent a floor.
        }

        // Independent wall probe used for I-Ninja climb/run/kick decisions. It
        // does not depend on horizontal displacement, so jumping straight up at
        // a wall still detects contact. Eight directions x two body heights fit
        // the engine's 16-trace batch limit.
        public bool TryWallProbe(uint player, Vec3 position, Vec3 facing, float scale,
                                 out bool contact, out Vec3 normal, out float facingDot)
        {
            contact = false; normal = Vec3.Zero; facingDot = 0.0f;
            if (!Valid(position, scale) || !Finite(facing)) return false;
            Vec3 forward = new Vec3(facing.X, 0.0f, facing.Z);
            if (forward.LengthSquared < 1e-6f) return false;
            forward = forward.Normalized();
            int index = 0;
            for (int h = 0; h < 2; h++) {
                float y = position.Y + (h == 0 ? WallProbeLow : WallProbeHigh) * scale;
                for (int d = 0; d < 8; d++) {
                    double angle = d * Math.PI * 0.25;
                    var direction = new Vec3((float)Math.Cos(angle), 0.0f, (float)Math.Sin(angle));
                    Vec3 start = new Vec3(position.X, y, position.Z);
                    _wall[index++] = Segment(start, start + direction * ((Radius + WallReach) * scale));
                }
            }
            if (!_trace(player, _wall, _wallHits)) return false;
            float bestDistance = float.PositiveInfinity;
            Vec3 bestNormal = Vec3.Zero; float bestFacing = 0.0f;
            for (int i = 0; i < 16; i++) {
                if (!_wallHits[i].HasHit) continue;
                if (!Contact(_wall[i], _wallHits[i], scale, out Vec3 n)) return false;
                // Wall or steep half-pipe, but not a walkable floor or a ceiling.
                if (n.Y < 0.0f || n.Y >= WalkableNormalY) continue;
                Vec3 horizontal = new Vec3(n.X, 0.0f, n.Z);
                if (horizontal.LengthSquared < 0.04f) continue;
                horizontal = horizontal.Normalized();
                float dot = Vec3.Dot(forward, -horizontal);
                if (dot < WallFacingMin) continue;
                Vec3 start = new Vec3(_wall[i].StartX, _wall[i].StartY, _wall[i].StartZ);
                Vec3 point = new Vec3(_wallHits[i].PointX, _wallHits[i].PointY, _wallHits[i].PointZ);
                float distance = (point - start).Length;
                if (distance < bestDistance ||
                    (Math.Abs(distance - bestDistance) <= 0.001f * scale && dot > bestFacing)) {
                    bestDistance = distance; bestNormal = n; bestFacing = dot;
                }
            }
            if (float.IsInfinity(bestDistance)) return true;
            contact = true; normal = bestNormal.Normalized(); facingDot = bestFacing;
            return true;
        }

        // Highest surface under the footprint within one step up or down. Used
        // while grounded to follow slopes/platform edges and the lower half-pipe
        // instead of stepping into the surface and losing support. Steep hits
        // (>= 0.5) are reported so wall-run can take over at the curve.
        public bool TryStepSupport(uint player, Vec3 position, float scale,
                                   out bool supported, out float groundY, out Vec3 normal)
        {
            supported = false; groundY = float.NegativeInfinity; normal = new Vec3(0, 1, 0);
            if (!Valid(position, scale)) return false;
            for (int i = 0; i < 5; i++) {
                float x = position.X + OffsetX[i] * Radius * scale, z = position.Z + OffsetZ[i] * Radius * scale;
                _step[i] = new TraceSegment(x, position.Y + StepHeight * scale, z,
                                            x, position.Y - StepHeight * scale, z);
            }
            if (!_trace(player, _step, _stepHits)) return false;
            float highest = float.NegativeInfinity; Vec3 bestNormal = new Vec3(0, 1, 0);
            for (int i = 0; i < 5; i++) {
                if (!_stepHits[i].HasHit) continue;
                if (!Contact(_step[i], _stepHits[i], scale, out Vec3 n)) return false;
                if (n.Y < SteepNormalY) continue;
                float y = _stepHits[i].PointY;
                if (y - position.Y > StepHeight * scale + 0.001f ||
                    position.Y - y > StepHeight * scale + 0.001f) continue;
                if (y > highest) { highest = y; bestNormal = n; }
            }
            if (float.IsInfinity(highest)) return true;
            supported = true; groundY = highest; normal = bestNormal.Normalized();
            return true;
        }

        public bool TryHorizontal(uint player, Vec3 from, Vec3 displacement, float scale, ref Vec3 velocity,
                                  out Vec3 position, out bool wall, out Vec3 wallNormal,
                                  float blockBelowNormalY = SteepNormalY)
        {
            position = from; wall = false; wallNormal = Vec3.Zero;
            if (!Valid(from, scale) || !Finite(displacement) || !Finite(velocity) ||
                !Finite(blockBelowNormalY) || blockBelowNormalY < 0.0f || blockBelowNormalY > 1.0f) return false;
            Vec3 remaining = new Vec3(displacement.X, 0, displacement.Z);
            int planeCount = 0;
            for (int bump = 0; bump < 3; bump++) {
                float length = remaining.Length;
                if (length <= 0.001f * scale) return true;
                Vec3 direction = remaining / length, side = new Vec3(-direction.Z, 0, direction.X);
                int index = 0;
                // Trace from the body center to beyond its leading radius.
                // Starting only at the leading edge misses existing overlap.
                for (int h = 0; h < 3; h++) for (int s = -1; s <= 1; s++) {
                    Vec3 start = position + side * (s * Radius * scale) + new Vec3(0, BodyHeight[h] * scale, 0);
                    Vec3 end = start + direction * (length + (Radius + Skin) * scale);
                    _body[index++] = Segment(start, end);
                }
                if (!_trace(player, _body, _bodyHits)) return false;
                float safeDistance = length; int best = -1; Vec3 bestNormal = Vec3.Zero;
                for (int i = 0; i < 9; i++) {
                    if (!_bodyHits[i].HasHit) continue;
                    if (!Contact(_body[i], _bodyHits[i], scale, out Vec3 n)) return false;
                    // Walkable slopes and the lower half-pipe must not stop
                    // horizontal motion like a wall; step support lifts the
                    // body over them. Wall mode passes 0.7 here so steep
                    // surfaces still constrain its stick/run motion.
                    if (n.Y >= blockBelowNormalY) continue;
                    Vec3 horizontalNormal = new Vec3(n.X, 0, n.Z).Normalized();
                    if (Vec3.Dot(direction, horizontalNormal) >= -0.0001f) continue;
                    Vec3 start = new Vec3(_body[i].StartX, _body[i].StartY, _body[i].StartZ);
                    Vec3 point = new Vec3(_bodyHits[i].PointX, _bodyHits[i].PointY, _bodyHits[i].PointZ);
                    float distance = Math.Max(0, Vec3.Dot(point - start, direction) - (Radius + Skin) * scale);
                    if (distance < safeDistance) { safeDistance = distance; best = i; bestNormal = n; }
                }
                position += direction * safeDistance;
                if (best < 0) return Finite(position);
                wall = true; wallNormal = bestNormal;
                Vec3 blockNormal = new Vec3(bestNormal.X, 0, bestNormal.Z).Normalized();
                _planes[planeCount++] = blockNormal;
                remaining -= direction * safeDistance;
                // Clip the unconsumed motion AND velocity, then sweep the slide
                // again. Never append an untraced tangent at a wall/corner.
                for (int pass = 0; pass < 3; pass++) for (int p = 0; p < planeCount; p++) {
                    remaining = ClipInto(remaining, _planes[p]);
                    velocity = ClipInto(velocity, _planes[p]);
                }
                for (int p = 0; p < planeCount; p++) if (Vec3.Dot(remaining, _planes[p]) < -0.001f * scale) {
                    remaining = Vec3.Zero; velocity.X = velocity.Z = 0; break;
                }
            }
            return Finite(position); // Exhausted budget: discard remaining motion.
        }

        public bool TryVertical(uint player, Vec3 from, float targetY, float scale, out float resultY, out bool floor, out bool ceiling)
        {
            resultY = from.Y; floor = ceiling = false;
            if (!Valid(from, scale) || !Finite(targetY)) return false;
            float dy = targetY - from.Y;
            if (dy == 0) return true;
            bool up = dy > 0;
            float offset = (up ? Height : Skin) * scale;
            for (int i = 0; i < 5; i++) {
                float x = from.X + OffsetX[i] * Radius * scale, z = from.Z + OffsetZ[i] * Radius * scale;
                // Include the skin at the leading end too. Otherwise a small
                // final descent can put the root below a floor while its padded
                // foot ray still ends above it, trapping the next landing.
                float endY = targetY + offset + (up ? Skin : -Skin) * scale;
                _vertical[i] = new TraceSegment(x, from.Y + offset, z, x, endY, z);
            }
            if (!_trace(player, _vertical, _verticalHits)) return false;
            float distance = Math.Abs(dy), safeDistance = distance;
            for (int i = 0; i < 5; i++) {
                if (!_verticalHits[i].HasHit) continue;
                if (!Contact(_vertical[i], _verticalHits[i], scale, out Vec3 n)) return false;
                if (n.Y * dy >= -0.0001f * scale) continue;
                float hitDistance = Math.Abs(_verticalHits[i].PointY - _vertical[i].StartY);
                float safe = Math.Max(0, hitDistance - Skin * scale);
                if (safe <= safeDistance) { safeDistance = safe; floor = !up && n.Y >= 0.7f; ceiling = up; }
            }
            resultY = from.Y + (up ? safeDistance : -safeDistance);
            return Finite(resultY);
        }

        // Native corrections/platform motion are the anchor. Replacement only
        // credits already-completed motion in the requested direction; it never
        // rewinds a correction to last tick's position.
        internal static Vec3 RemainingHorizontal(Vec3 desired, Vec3 nativeDelta, bool replace)
        {
            desired.Y = 0; nativeDelta.Y = 0;
            if (!replace || desired.Length <= 1e-8f) return desired;
            Vec3 direction = desired.Normalized();
            float credit = Math.Max(0, Math.Min(desired.Length, Vec3.Dot(nativeDelta, direction)));
            return desired - direction * credit;
        }

        internal static Vec3 ClipInto(Vec3 velocity, Vec3 normal)
        {
            float into = Vec3.Dot(velocity, normal);
            return into < 0 ? velocity - normal * into : velocity;
        }

        private static TraceSegment Segment(Vec3 start, Vec3 end) => new TraceSegment(start.X, start.Y, start.Z, end.X, end.Y, end.Z);
        private static bool Contact(TraceSegment segment, TraceHit hit, float scale, out Vec3 normal)
        {
            Vec3 point = new Vec3(hit.PointX, hit.PointY, hit.PointZ);
            normal = new Vec3(hit.NormalX, hit.NormalY, hit.NormalZ);
            if (!Finite(point) || !Finite(normal) || normal.LengthSquared < 0.25f || normal.LengthSquared > 2.25f) return false;
            Vec3 start = new Vec3(segment.StartX, segment.StartY, segment.StartZ);
            Vec3 delta = new Vec3(segment.EndX, segment.EndY, segment.EndZ) - start;
            float fraction = Vec3.Dot(point - start, delta) / delta.LengthSquared;
            float tolerance = Skin * scale + 0.00001f;
            if (!Finite(fraction) || fraction < -0.001f || fraction > 1.001f ||
                (point - (start + delta * fraction)).Length > tolerance) return false;
            normal = normal.Normalized(); return true;
        }
        private static bool Valid(Vec3 position, float scale) => Finite(position) && Finite(scale) && scale >= 0.001f && scale <= 1;
        private static bool Finite(Vec3 v) => Finite(v.X) && Finite(v.Y) && Finite(v.Z);
        private static bool Finite(float v) => !float.IsNaN(v) && !float.IsInfinity(v);
    }
}
