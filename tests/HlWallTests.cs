using System;
using HalfLifeMovement;

internal static class HlWallTests
{
    private static int checks;
    private static void Check(bool ok, string label) { checks++; if (!ok) throw new Exception(label); }
    private static void Near(float actual, float expected, string label, float epsilon = 0.001f)
    {
        Check(!float.IsNaN(actual) && !float.IsInfinity(actual) && Math.Abs(actual - expected) <= epsilon,
              label + ": expected " + expected + ", got " + actual);
    }

    public static int Main()
    {
        try
        {
            var tune = new HlTuning();
            var wall = new HlWall();
            var up = new Vec3(0, 1, 0);
            var view = new Vec3(0, 0, 1);
            var wallLeft = new Vec3(-1, 0, 0); // plane facing +X; player is on its +X side
            const float dt = 1.0f / 60.0f;

            Check(HlWall.WallLike(wallLeft), "vertical wall is wall-like");
            Check(HlWall.WallLike(new Vec3(0, 0.5f, -0.866f)), "steep half-pipe is wall-like");
            Check(!HlWall.WallLike(up), "floor is not a wall");
            Check(!HlWall.WallLike(new Vec3(0, -1, 0)), "ceiling is not a wall");
            Check(!HlWall.WallLike(new Vec3(0, 0.8f, -0.6f)), "walkable slope is not a wall");
            Check(!HlWall.WallLike(new Vec3(float.NaN, 0, 0)), "non-finite normal refused");
            Check(!HlWall.WallLike(new Vec3(0, 0, 0)), "zero normal refused");
            Check(!wall.TryAttach(up, view, Vec3.Zero, tune), "cannot attach a floor");
            Check(!wall.TryAttach(new Vec3(0, -1, 0), view, Vec3.Zero, tune), "cannot attach a ceiling");
            Check(!wall.TryAttach(wallLeft, view, Vec3.Zero, null), "null tuning refused");

            // Slow contact starts a climb. The tangent follows the view along
            // the wall and the stick component pushes into the wall plane.
            Check(wall.TryAttach(wallLeft, view, Vec3.Zero, tune), "attach vertical wall");
            Check(wall.Mode == WallMode.Climb, "slow wall contact climbs");
            Near(Vec3.Dot(wall.Tangent, view), 1, "tangent follows view");
            Check(wall.Tick(dt, true, true, Vec3.Zero, tune.MaxSpeed, out Vec3 v) == WallTickResult.Active,
                  "climb tick active");
            Check(v.Y > 0, "climb gains height");
            Check(v.X > 0, "climb sticks into the wall");
            float firstY = v.Y;
            for (int i = 0; i < 24; i++)
                Check(wall.Tick(dt, true, true, Vec3.Zero, tune.MaxSpeed, out v) == WallTickResult.Active,
                      "climb remains active");
            Check(v.Y > firstY + 50, "held climb accelerates upward");
            Near(Vec3.Dot(new Vec3(v.X, 0, v.Z), new Vec3(0, 0, 1)), 0, "climb tangent is along the wall");

            // Fast contact attaches as a wall run and starts with upward
            // velocity, then gravity reduces it while contact is held.
            wall.Reset();
            Check(wall.TryAttach(wallLeft, view, new Vec3(0, 0, 300), tune), "attach fast wall contact");
            Check(wall.Mode == WallMode.Run, "fast wall contact runs");
            Check(wall.Tick(dt, true, true, new Vec3(0, 0, 320), tune.MaxSpeed, out v) == WallTickResult.Active,
                  "run tick active");
            Check(v.Y > 0 && v.Y < HlWall.RunInitialUp, "run starts rising then decays");
            float previousY = v.Y;
            for (int i = 0; i < 12; i++)
                wall.Tick(dt, true, false, new Vec3(0, 0, 320), tune.MaxSpeed, out v);
            Check(v.Y < previousY, "run vertical speed decays");
            Check(v.Y >= -HlWall.SlideSpeed - 0.001f, "run vertical speed clamps at slide speed");

            // Releasing the climb key after the grace period slides instead of
            // continuing to gain height.
            wall.Reset();
            Check(wall.TryAttach(wallLeft, view, Vec3.Zero, tune), "reattach climb");
            for (int i = 0; i < HlWall.ClimbTicks; i++)
                Check(wall.Tick(dt, true, false, Vec3.Zero, tune.MaxSpeed, out v) == WallTickResult.Active,
                      "climb lasts through its grace/timer");
            Check(v.Y < 0, "released climb slides downward");

            // Losing contact for more than the tolerance detaches, carrying the
            // last wall velocity into the fall.
            wall.Reset();
            Check(wall.TryAttach(wallLeft, view, Vec3.Zero, tune), "attach for loss test");
            Check(wall.Tick(dt, false, true, Vec3.Zero, tune.MaxSpeed, out v) == WallTickResult.Active, "first lost contact tolerated");
            Check(wall.Tick(dt, false, true, Vec3.Zero, tune.MaxSpeed, out v) == WallTickResult.Active, "second lost contact tolerated");
            Check(wall.Tick(dt, false, true, Vec3.Zero, tune.MaxSpeed, out v) == WallTickResult.Detached, "third lost contact detaches");
            Check(!wall.Active, "detached state cleared");

            // The climb timer expires instead of holding ownership forever.
            wall.Reset();
            Check(wall.TryAttach(wallLeft, view, Vec3.Zero, tune), "attach for timer test");
            for (int i = 0; i < HlWall.ClimbTicks; i++)
                Check(wall.Tick(dt, true, true, Vec3.Zero, tune.MaxSpeed, out v) == WallTickResult.Active,
                      "climb ticks within duration");
            Check(wall.Tick(dt, true, true, Vec3.Zero, tune.MaxSpeed, out v) == WallTickResult.Detached,
                  "climb timer expires");
            Check(!wall.Active, "expired climb cleared");

            // A second Space kicks away from the wall and locks reattachment.
            wall.Reset();
            Check(wall.TryAttach(wallLeft, view, Vec3.Zero, tune), "attach for kick test");
            Vec3 kick = wall.Kick(out float kickUp);
            Near(kick.X, -HlWall.KickOut, "kick pushes away from wall");
            Near(kick.Z, 0, "climb kick has no tangent impulse");
            Near(kickUp, HlWall.KickUp, "kick vertical impulse");
            Check(!wall.Active, "kick ends wall state");
            Check(!wall.TryAttach(wallLeft, view, Vec3.Zero, tune), "kick lock refuses immediate reattach");
            for (int i = 0; i < HlWall.KickLockTicks; i++) wall.TickLock();
            Near(wall.KickLock, 0, "kick lock expires");
            Check(wall.TryAttach(wallLeft, view, Vec3.Zero, tune), "reattach after kick lock");

            // Half-pipe: a steep surface is accepted; climbing and sticking use
            // its plane normal, not a purely vertical wall assumption.
            wall.Reset();
            var halfPipe = new Vec3(0, 0.5f, -0.866f).Normalized();
            Check(wall.TryAttach(halfPipe, view, Vec3.Zero, tune), "attach half-pipe curve");
            Check(wall.Tick(dt, true, true, Vec3.Zero, tune.MaxSpeed, out v) == WallTickResult.Active,
                  "half-pipe climb active");
            Check(v.Y > 0, "half-pipe climb gains height");
            Check(v.Z > 0, "half-pipe stick follows surface normal into the curve");

            // Invalid tick inputs detach instead of producing NaN positions.
            wall.Reset();
            Check(wall.TryAttach(wallLeft, view, Vec3.Zero, tune), "attach for invalid tick");
            Check(wall.Tick(float.NaN, true, true, Vec3.Zero, tune.MaxSpeed, out v) == WallTickResult.Detached,
                  "NaN dt detaches");
            Check(!wall.Active, "invalid tick clears state");

            Console.WriteLine("PASS I-Ninja wall state: " + checks + " assertions (synthetic, no live acceptance)");
            return 0;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine(ex);
            return 1;
        }
    }
}
