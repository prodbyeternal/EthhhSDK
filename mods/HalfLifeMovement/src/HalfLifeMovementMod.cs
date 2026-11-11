// ===========================================================================
//  Half-Life Movement - EthhhSDK C# showcase mod
//
//  WHAT IT DOES
//    * Replaces the horizontal player movement with Valve HLSDK pmove physics
//      (ground friction/acceleration, air acceleration, bunnyhop cap, HL jump
//      and gravity). Port lives in HlMove.cs.
//    * Provides a mouse-look FPS camera with configurable eye height, FOV and
//      sensitivity. The game camera is overridden after the game's own tick,
//      so the engine's camera scripts cannot fight it until you disable the mod.
//    * Runtime-tunable movement variables, live diagnostics central panel.
//
//  HOTKEYS
//    F2  activate / deactivate
//    F3  deactivate + resync (panic key)
//
//  IMPORTANT
//    * The default mod jump key is V, NOT Space: I-Ninja's own jump usually
//      listens to Space. Native takeoffs have priority; use a separate key for
//      a predictable HL impulse. Rebind for your keyboard/game configuration.
//    * The player tracker must know which strat is the ninja. The Trainer mod
//      ("Player" section) can pin it; this mod also has a manual address box.
//    * This is an experimental position-write adapter, not a verified hull-
//      trace port. Collision corrections may be bypassed. Test in open areas
//      and use F3 to stop. Pin the correct player strat before activating.
//
//  Based on Valve's halflife-steam_legacy movement code (pm_shared.c).
//  Final agreement/attribution documentation is pending; local testing only.
// ===========================================================================
using System;
using System.Globalization;
using EthhhSDK;
using EthhhSDK.Modding;

namespace HalfLifeMovement
{
    public sealed class HalfLifeMovementMod : EthModBase
    {
        // ------------------------------------------------------------- palette
        private static readonly uint ColAccent = ImGui.RGBA(0.44f, 0.58f, 1.00f, 1.00f);
        private static readonly uint ColOk     = ImGui.RGBA(0.36f, 0.85f, 0.55f, 1.00f);
        private static readonly uint ColWarn   = ImGui.RGBA(1.00f, 0.71f, 0.28f, 1.00f);
        private static readonly uint ColBad    = ImGui.RGBA(1.00f, 0.42f, 0.42f, 1.00f);
        private static readonly uint ColDim    = ImGui.RGBA(0.55f, 0.59f, 0.66f, 1.00f);

        private const float DegToRad = (float)(Math.PI / 180.0);
        private const float RadToDeg = (float)(180.0 / Math.PI);

        private static readonly string[] JumpKeyNames = { "V", "Space", "C", "X", "Z", "B", "Left Ctrl", "Left Shift" };
        private static readonly int[]    JumpKeyCodes = { Keys.V, Keys.Space, Keys.C, Keys.X, Keys.Z, Keys.B, Keys.LeftCtrl, Keys.LeftShift };

        // ------------------------------------------------------------- runtime
        private readonly HlMove _move = new HlMove();
        private readonly HlTuning _tune = new HlTuning();
        private readonly float[] _camMatrix = new float[16];
        private readonly float[] _speedHistory = new float[180];

        private bool _active;
        private bool _movementEnabled = true;
        private bool _cameraEnabled = true;
        private bool _replaceGameMovement = true;
        private bool _hlVertical = true;
        private bool _autoHop;
        private bool _capBunnySpeed = true;
        private bool _captureMouse = true;
        private bool _invertY;
        private bool _captureOwned;

        private float _sensitivity = 0.030f;   // degrees per raw mouse count
        private float _eyeHeight = 69.0f;
        private float _forwardOffset = 24.0f;
        // Starting estimate, NOT a measured I-Ninja unit convention. Keep the
        // HL physics in HL units and convert only at the game boundary.
        private float _worldScale = 0.025f;
        private float _fov = 90.0f;
        private float _savedFovDegrees;

        private int _jumpKey = Keys.V;
        private bool _jumpHeld;
        private bool _f2Held;
        private bool _f3Held;

        private float _yaw;
        private float _pitch;

        // player / pmove state
        private uint _playerAddr;
        private uint _playerClass;
        private uint _playerHandle;
        private bool _fovOwned;
        private bool _haveWritten;
        private Vec3 _lastGame;
        private Vec3 _lastWritten;
        private float _gameVy;
        private float _jumpCarry;
        private float _jumpVel;
        private bool _hlJumpActive;
        private bool _nativeVerticalActive;
        private bool _grounded = true;
        private float _speed;
        private int _failTicks;
        private int _nativeTraversalTicks;
        private int _traversalSupportTicks;
        private int _reacquireTicks;
        private Vec3 _wallNormal;
        private bool _hasWall;
        private readonly HlWall _wall = new HlWall();
        private bool _spaceHeld;
        private Vec3 _wallProbeNormal;
        private float _wallFacingDot;
        private readonly HlCollision _collision = new HlCollision(Game.TraceSegments);
        private bool _supportQueryValid;

        // settings persistence (throttled so slider drags do not hammer the INI)
        private bool _settingsDirty;
        private long _lastSaveTicks;
        private string _playerAddrText = "";

        // =====================================================================
        //  Lifecycle
        // =====================================================================
        public override void OnInitialize()
        {
            LoadSettings();
            _playerAddrText = _playerAddr != 0 ? _playerAddr.ToString("X8") : "";
            ModLog.Info("Half-Life movement loaded (F2 = enable, F3 = panic)");
            ModStatus.Set("standby (F2)");
        }

        private void LoadSettings()
        {
            // Never resume ownership of game state without activation/resync.
            _active              = false;
            _movementEnabled     = ModSettings.GetBool("movement", true);
            _cameraEnabled       = ModSettings.GetBool("camera", true);
            _replaceGameMovement = ModSettings.GetBool("replace_movement", true);
            _hlVertical          = ModSettings.GetBool("hl_vertical", true);
            _autoHop             = ModSettings.GetBool("autohop", false);
            _capBunnySpeed       = ModSettings.GetBool("bunny_cap", true);
            _captureMouse        = ModSettings.GetBool("capture_mouse", true);
            _invertY             = ModSettings.GetBool("invert_y", false);

            _sensitivity  = ModSettings.GetFloat("sensitivity", 0.030f);
            _eyeHeight    = ModSettings.GetFloat("eye_height", 69.0f);
            _forwardOffset= ModSettings.GetFloat("forward_offset", 24.0f);
            _worldScale   = ModSettings.GetFloat("world_units_per_hl_unit", 0.025f);
            _fov          = ModSettings.GetFloat("fov", 90.0f);

            _tune.Gravity              = ModSettings.GetFloat("gravity", 800.0f);
            _tune.MaxSpeed             = ModSettings.GetFloat("maxspeed", 320.0f);
            _tune.Accelerate           = ModSettings.GetFloat("accelerate", 10.0f);
            _tune.AirAccelerate        = ModSettings.GetFloat("airaccelerate", 10.0f);
            _tune.Friction             = ModSettings.GetFloat("friction", 4.0f);
            _tune.StopSpeed            = ModSettings.GetFloat("stopspeed", 100.0f);
            _tune.JumpSpeed            = ModSettings.GetFloat("jumpspeed", 268.328f);
            _tune.MaxVelocity          = ModSettings.GetFloat("maxvelocity", 2000.0f);
            _tune.GameVerticalThreshold= ModSettings.GetFloat("game_vertical_threshold", 180.0f);

            // INI values are editable outside the UI. Never feed non-finite or
            // out-of-range settings into a game position/camera write.
            _sensitivity = Bound(_sensitivity, 0.002f, 0.2f, 0.030f);
            _eyeHeight = Bound(_eyeHeight, 0.0f, 300.0f, 69.0f);
            _forwardOffset = Bound(_forwardOffset, 0.0f, 60.0f, 24.0f);
            _worldScale = Bound(_worldScale, 0.001f, 1.0f, 0.025f);
            _fov = Bound(_fov, 50.0f, 130.0f, 90.0f);
            _tune.Gravity = Bound(_tune.Gravity, 0.0f, 2400.0f, 800.0f);
            _tune.MaxSpeed = Bound(_tune.MaxSpeed, 60.0f, 1200.0f, 320.0f);
            _tune.Accelerate = Bound(_tune.Accelerate, 1.0f, 40.0f, 10.0f);
            _tune.AirAccelerate = Bound(_tune.AirAccelerate, 1.0f, 40.0f, 10.0f);
            _tune.Friction = Bound(_tune.Friction, 0.0f, 20.0f, 4.0f);
            _tune.StopSpeed = Bound(_tune.StopSpeed, 0.0f, 400.0f, 100.0f);
            _tune.JumpSpeed = Bound(_tune.JumpSpeed, 0.0f, 600.0f, 268.328f);
            _tune.MaxVelocity = Bound(_tune.MaxVelocity, 1.0f, 2000.0f, 2000.0f);
            _tune.GameVerticalThreshold = Bound(_tune.GameVerticalThreshold, 20.0f, 600.0f, 180.0f);

            _jumpKey = KeyCodeFromName(ModSettings.GetString("jump_key", "V"));
            _jumpHeld = Input.KeyDown(_jumpKey);
        }

        private void Touch() { _settingsDirty = true; }

        private void FlushSettings()
        {
            if (!_settingsDirty)
                return;

            ModSettings.SetBool("movement", _movementEnabled);
            ModSettings.SetBool("camera", _cameraEnabled);
            ModSettings.SetBool("replace_movement", _replaceGameMovement);
            ModSettings.SetBool("hl_vertical", _hlVertical);
            ModSettings.SetBool("autohop", _autoHop);
            ModSettings.SetBool("bunny_cap", _capBunnySpeed);
            ModSettings.SetBool("capture_mouse", _captureMouse);
            ModSettings.SetBool("invert_y", _invertY);

            ModSettings.SetFloat("sensitivity", _sensitivity);
            ModSettings.SetFloat("eye_height", _eyeHeight);
            ModSettings.SetFloat("forward_offset", _forwardOffset);
            ModSettings.SetFloat("world_units_per_hl_unit", _worldScale);
            ModSettings.SetFloat("fov", _fov);

            ModSettings.SetFloat("gravity", _tune.Gravity);
            ModSettings.SetFloat("maxspeed", _tune.MaxSpeed);
            ModSettings.SetFloat("accelerate", _tune.Accelerate);
            ModSettings.SetFloat("airaccelerate", _tune.AirAccelerate);
            ModSettings.SetFloat("friction", _tune.Friction);
            ModSettings.SetFloat("stopspeed", _tune.StopSpeed);
            ModSettings.SetFloat("jumpspeed", _tune.JumpSpeed);
            ModSettings.SetFloat("maxvelocity", _tune.MaxVelocity);
            ModSettings.SetFloat("game_vertical_threshold", _tune.GameVerticalThreshold);

            ModSettings.SetString("jump_key", KeyNameFromCode(_jumpKey));
            ModSettings.Save();
            _settingsDirty = false;
            _lastSaveTicks = DateTime.UtcNow.Ticks;
        }

        private static int KeyCodeFromName(string name)
        {
            if (!string.IsNullOrEmpty(name))
            {
                for (int i = 0; i < JumpKeyNames.Length; i++)
                    if (string.Equals(JumpKeyNames[i], name, StringComparison.OrdinalIgnoreCase))
                        return JumpKeyCodes[i];
            }
            return Keys.V;
        }

        private static string KeyNameFromCode(int code)
        {
            int index = Array.IndexOf(JumpKeyCodes, code);
            return index >= 0 ? JumpKeyNames[index] : "V";
        }

        // =====================================================================
        //  Activation
        // =====================================================================
        private void Activate()
        {
            if (_active)
                return;
            if (!Game.IsReady)
            {
                ModLog.Warn("game is not ready yet (no level loaded?)");
                return;
            }
            if (!_movementEnabled && !_cameraEnabled)
            {
                ModLog.Warn("enable movement and/or camera first");
                return;
            }
            if (!Game.PlayerPinned)
            {
                ModLog.Warn("select/pin the ninja strat in the Trainer or Player tracking before activating");
                return;
            }

            if (!Game.TryReadStratIdentity(Game.PlayerAddr, out StratIdentity identity) || !identity.IsExactNinja)
            {
                ModLog.Warn("HL requires exact class Ninja; attachments, camera, speech and unreadable identities are refused");
                return;
            }

            _active = true;
            Game.SetExternalCameraOwned(_cameraEnabled);
            _fovOwned = false;
            _savedFovDegrees = Game.TryReadCamera(out CameraInfo cam) ? cam.Fov * RadToDeg : 0.0f;

            if (!ResyncToPlayer())
            {
                _active = false;
                Game.SetExternalCameraOwned(false);
                ModLog.Warn("no player strat tracked - use the Trainer to select the ninja");
                return;
            }

            InitLookFromCamera();
            Input.ConsumeMouseDelta();
            EnsureCapture(true);
            _jumpHeld = Input.KeyDown(_jumpKey);

            ModLog.Info(string.Format("activated (movement={0} camera={1})",
                _movementEnabled ? 1 : 0, _cameraEnabled ? 1 : 0));
            ModStatus.Set("active (F2 toggles)");
            Touch();
        }

        private void Deactivate()
        {
            Input.LeaseMovementKeys(0, false);
            if (!_active)
                return;

            _active = false;
            Game.SetExternalCameraOwned(false);
            if (_captureOwned)
            {
                Input.SetCapture(false);
                _captureOwned = false;
            }
            if (_fovOwned && _savedFovDegrees >= 1.0f)
                Game.FovDegrees = _savedFovDegrees;
            _fovOwned = false;

            _move.Reset();
            ResetVertical();
            _wall.Reset();
            _haveWritten = false;

            ModLog.Info("deactivated");
            ModStatus.Set("standby (F2)");
            Touch();
            FlushSettings();
        }

        private bool ResyncToPlayer()
        {
            if (!Game.TryGetPlayerPosition(out float x, out float y, out float z))
                return false;
            if (!Finite(x) || !Finite(y) || !Finite(z) || !Game.TryReadPlayer(out StratInfo strat) ||
                !Game.TryReadStratIdentity(strat.Addr, out StratIdentity identity) || !identity.IsExactNinja ||
                identity.Class != strat.Class || identity.SelfHandle == 0)
                return false;

            _playerAddr = Game.PlayerAddr;
            _playerClass = strat.Class;
            _playerHandle = identity.SelfHandle;
            var pos = new Vec3(x, y, z);
            _lastGame = pos;
            _lastWritten = pos;
            _haveWritten = true;

            _move.Reset();
            ResetVertical();
            _wall.Reset();
            _spaceHeld = Input.KeyDown(Keys.Space);
            _gameVy = 0.0f;
            _grounded = ProbeGround(pos, out float _, out Vec3 _);
            _speed = 0.0f;
            _failTicks = 0;
            _nativeTraversalTicks = 0;
            _traversalSupportTicks = 0;
            _reacquireTicks = 0;
            return _supportQueryValid;
        }

        private void ResetVertical()
        {
            _jumpCarry = _jumpVel = 0;
            _hlJumpActive = _nativeVerticalActive = false;
        }

        private bool ProbeGround(Vec3 position, out float groundY, out Vec3 normal)
        {
            _supportQueryValid = _collision.TryGround(_playerAddr, position, _worldScale, out bool supported, out groundY, out normal);
            return _supportQueryValid && supported;
        }

        private bool ProbeWall(Vec3 position, Vec3 facing, out Vec3 normal, out float facingDot)
        {
            normal = Vec3.Zero; facingDot = 0.0f;
            if (!_collision.TryWallProbe(_playerAddr, position, facing, _worldScale,
                                         out bool contact, out normal, out facingDot))
                return false;
            if (!contact) { normal = Vec3.Zero; facingDot = 0.0f; return false; }
            return true;
        }

        private void InitLookFromCamera()
        {
            // The engine camera normally looks at the ninja. Use that direction
            // for an initial yaw/pitch so enabling FPS mode does not snap the view.
            if (!Game.TryGetPlayerPosition(out float px, out float py, out float pz))
                return;
            if (!Game.TryReadCamera(out CameraInfo cam))
                return;

            var target = new Vec3(px, py + Math.Max(1.0f, _eyeHeight) * _worldScale, pz);
            var fwd = new Vec3(target.X - cam.X, target.Y - cam.Y, target.Z - cam.Z);
            if (fwd.Length < 5.0f * _worldScale)
                return;

            fwd = fwd.Normalized();
            _yaw = (float)Math.Atan2(fwd.X, fwd.Z);
            _pitch = (float)Math.Asin(Clamp(fwd.Y, -1.0f, 1.0f));
        }

        private void EnsureCapture(bool force)
        {
            bool want = _active && _cameraEnabled && _captureMouse && !Input.OverlayOpen;
            if (want && (!_captureOwned || force))
            {
                // Track the request even when focus temporarily suspends the
                // native capture. Cleanup must cancel that pending request.
                Input.SetCapture(true);
                _captureOwned = true;
            }
            else if (!want && _captureOwned)
            {
                Input.SetCapture(false);
                _captureOwned = false;
            }
        }

        // =====================================================================
        //  Tick
        // =====================================================================
        public override void OnTick(int tickIndex)
        {
            // Persist settings once a second instead of on every slider frame.
            if (_settingsDirty && DateTime.UtcNow.Ticks - _lastSaveTicks >= TimeSpan.TicksPerSecond)
                FlushSettings();

            HandleHotkeys();

            if (!_active)
                return;
            if (!Game.IsReady) { Deactivate(); return; }
            if (Input.OverlayOpen)
            {
                _jumpHeld = Input.KeyDown(_jumpKey);
                Input.ConsumeMouseDelta();
                return;
            }

            EnsureCapture(false);

            if (!Game.TryGetPlayerPosition(out float px, out float py, out float pz))
            {
                if (++_failTicks % 30 == 0 && Game.PlayerReacquireExactNinja() && ResyncToPlayer()) return;
                if (_failTicks == 180)
                {
                    ModLog.Warn("player strat lost - reselect the ninja in the Trainer");
                    Deactivate();
                }
                return;
            }
            _failTicks = 0;

            uint addr = Game.PlayerAddr;
            if(!Game.PlayerPinned||addr==0||!Game.TryReadPlayer(out StratInfo currentStrat))
            {
                if(++_reacquireTicks%30==0 && Game.PlayerReacquireExactNinja() && ResyncToPlayer()) { ModLog.Info("exact Ninja reacquired after death/respawn");return; }
                if(_reacquireTicks>=180){ModLog.Warn("exact Ninja was not uniquely reacquired; stopped");Deactivate();}
                return;
            }
            if(addr!=_playerAddr||!_haveWritten||currentStrat.Class!=_playerClass||
                !Game.TryReadStratIdentity(addr,out StratIdentity identity)||!identity.IsExactNinja||
                identity.Class!=_playerClass||identity.SelfHandle!=_playerHandle||!Finite(px)||!Finite(py)||!Finite(pz))
            { ModLog.Warn("tracked player identity changed; stopped to avoid reuse");Deactivate();return; }
            _reacquireTicks=0;

            float dt = Game.TimeStep;
            if (!Finite(dt) || dt > 0.1f)
            {
                ModLog.Warn("unsupported movement time step; stopped");
                Deactivate();
                return;
            }
            if (!(dt > 0.0f))
            {
                Input.ConsumeMouseDelta();
                return;
            }

            var gamePos = new Vec3(px, py, pz);

            // Yield to native traversal/death/rail/chain motion without requiring
            // a mode toggle. Complete identity checks above still guard reuse.
            float ex = gamePos.X - _lastWritten.X;
            float ey = gamePos.Y - _lastWritten.Y;
            float ez = gamePos.Z - _lastWritten.Z;
            float teleportLimit = 300.0f * _worldScale;
            if (ex * ex + ey * ey + ez * ez > teleportLimit * teleportLimit)
            {
                _nativeTraversalTicks=30;_traversalSupportTicks=0;_move.Reset();ResetVertical();_wall.Reset();_lastWritten=gamePos;_lastGame=gamePos;
                Input.LeaseMovementKeys(0, false);
                _jumpHeld=Input.KeyDown(_jumpKey);
                _spaceHeld=Input.KeyDown(Keys.Space);
                if(_cameraEnabled) ApplyCamera(gamePos);
                return;
            }

            // Remove the displacement we ourselves supplied last tick.
            _gameVy = (gamePos.Y - _lastWritten.Y) / (dt * _worldScale);
            UpdateMouseLook();
            bool jumpDown = Input.KeyDown(_jumpKey);
            bool wasJumpHeld = _jumpHeld;
            _jumpHeld = jumpDown;
            bool spaceDown = Input.KeyDown(Keys.Space);
            bool spacePressed = spaceDown && !_spaceHeld;
            _spaceHeld = spaceDown;
            _wall.TickLock();

            bool onGround=ProbeGround(gamePos,out float groundY,out Vec3 groundNormal);
            if (!_supportQueryValid) { ModLog.Warn("support query failed/invalid; stopped without a position write"); Deactivate(); return; }

            // A native takeoff has priority, even while it is still within the
            // support ray's reach. Never pin its first upward frames to a floor.
            float gap = gamePos.Y - groundY;
            bool nativeRising = ey > 0.01f * _worldScale && (!onGround || gap > 0.01f * _worldScale);
            if (nativeRising) { ResetVertical(); _nativeVerticalActive = true; }
            // The longer probe finds nearby support; only skin-close contact
            // can grant friction/jump permission. Do not ground a short fall or
            // an activation that happens to be hovering inside the probe range.
            onGround = onGround && gap <= HlCollision.Skin * _worldScale;
            bool fastFall = _gameVy < -_tune.GameVerticalThreshold && gap > HlCollision.Skin * _worldScale;
            if (nativeRising || fastFall) onGround = false;
            if (_nativeVerticalActive && onGround && gap <= HlCollision.Skin * _worldScale && !nativeRising && _gameVy <= 0) _nativeVerticalActive = false;

            bool nativeUse = Input.KeyDown(Keys.Ctrl);
            if (nativeUse) { _nativeTraversalTicks = 30; _traversalSupportTicks = 0; }
            if (_nativeTraversalTicks > 0) {
                // Translation is not evidence of still owning a chain/rail: WASD
                // walking used to refresh this forever after Ctrl was released.
                if (!nativeUse) {
                    _nativeTraversalTicks--;
                    _traversalSupportTicks = onGround && !nativeRising && _gameVy <= 0 ? _traversalSupportTicks + 1 : 0;
                    if (_traversalSupportTicks >= 2) _nativeTraversalTicks = 0;
                }
                if (_nativeTraversalTicks > 0) {
                    Input.LeaseMovementKeys(0, false);
                    _move.Reset(); ResetVertical(); _wall.Reset();
                    // Carry native airborne authority across the grace timeout;
                    // being near a floor is not permission for an HL takeoff.
                    _nativeVerticalActive = !onGround;
                    _lastWritten = gamePos; _lastGame = gamePos;
                    _grounded = onGround;
                    Game.SetNinjaFacing(_yaw);
                    if (_cameraEnabled) ApplyCamera(gamePos);
                    return;
                }
            }

            // I-Ninja wall interaction. Space is read independently of the HL
            // jump key: at a wall the first press attaches a climb/wall run and
            // the next press kicks away. Ctrl still yields to native traversal.
            Vec3 viewDir = new Vec3((float)Math.Sin(_yaw), 0.0f, (float)Math.Cos(_yaw));
            bool wallContact = ProbeWall(gamePos, viewDir, out _wallProbeNormal, out _wallFacingDot);
            Vec3 intoWall = new Vec3(-_wallProbeNormal.X, 0.0f, -_wallProbeNormal.Z).Normalized();
            bool wallAttached = false, wallKicked = false;
            if (!nativeUse && _movementEnabled)
            {
                if (_wall.Active && spacePressed)
                {
                    Vec3 kick = _wall.Kick(out float kickUp);
                    _move.Velocity = new Vec3(kick.X, 0.0f, kick.Z);
                    _jumpVel = kickUp;
                    _jumpCarry = 0.0f;
                    _hlJumpActive = _hlVertical;
                    _nativeVerticalActive = false;
                    wallKicked = true;
                }
                else if (!_wall.Active && wallContact && spacePressed &&
                         _wall.KickLock == 0 && _wallFacingDot >= 0.2f &&
                         _wall.TryAttach(_wallProbeNormal, viewDir, _move.Velocity, _tune))
                {
                    wallAttached = true;
                }
                else if (!_wall.Active && !spacePressed && wallContact && onGround &&
                         _wall.KickLock == 0 &&
                         _wallProbeNormal.Y >= HlCollision.SteepNormalY &&
                         _move.Speed >= HlWall.RunMinSpeed &&
                         Vec3.Dot(viewDir, intoWall) >= 0.3f &&
                         Vec3.Dot(_move.Velocity, intoWall) >= 80.0f &&
                         _wall.TryAttach(_wallProbeNormal, viewDir, _move.Velocity, _tune))
                {
                    // Running fast into the curved part of a half-pipe starts a
                    // wall run without requiring a mode switch or another key.
                    wallAttached = true;
                }
            }
            if (wallAttached)
            {
                ResetVertical();
                _nativeVerticalActive = false;
                onGround = false;
                nativeRising = false;
            }
            else if (wallKicked)
            {
                _nativeVerticalActive = false;
                onGround = false;
                nativeRising = false;
            }

            // Height above takeoff is telemetry, NOT the flight/grounded flag.
            // An owned arc stays airborne until a downward sweep hits support.
            if (_hlJumpActive || _nativeVerticalActive || _wall.Active) onGround = false;
            bool wantJump = _movementEnabled && _hlVertical && !_hlJumpActive && !_nativeVerticalActive &&
                            !_wall.Active && onGround && groundNormal.Y >= HlCollision.WalkableNormalY &&
                            jumpDown && (_autoHop || !wasJumpHeld);
            bool jumped = false;

            if (wantJump)
            {
                _jumpVel = _tune.JumpSpeed;
                _hlJumpActive = true;
                _jumpCarry = 0;
                if (_capBunnySpeed)
                    _move.CapBunnySpeed(_tune);
                onGround = false;
                jumped = true;
            }

            if (_movementEnabled)
            {
                Vec3 wishVel = BuildWishVelocity();
                float wishSpeed = wishVel.Length;
                Vec3 wishDir = wishSpeed > 0.001f ? wishVel / wishSpeed : Vec3.Zero;
                if (wishSpeed > _tune.MaxSpeed)
                    wishSpeed = _tune.MaxSpeed;

                bool wallMove = _wall.Active;
                Vec3 wallVelocity = Vec3.Zero;
                if (wallMove)
                {
                    // Wall motion owns vertical and supersedes any native
                    // takeoff ownership for the duration of the climb/run.
                    _nativeVerticalActive = false;
                    _hlJumpActive = false;
                    WallTickResult wallResult = _wall.Tick(dt, wallContact,
                        spaceDown || Input.KeyDown(Keys.W), wishDir, _tune.MaxSpeed, out wallVelocity);
                    if (wallResult == WallTickResult.Detached)
                    {
                        // Finish this tick with the last wall velocity, then let
                        // the normal gravity/landing path resume.
                        _jumpVel = Clamp(wallVelocity.Y, -_tune.MaxVelocity, _tune.MaxVelocity);
                        _jumpCarry = 0.0f;
                        _hlJumpActive = _hlVertical;
                        _nativeVerticalActive = false;
                    }
                    _move.Velocity = new Vec3(wallVelocity.X, 0.0f, wallVelocity.Z);
                    onGround = false;
                }
                else if (onGround)
                {
                    _move.Velocity.Y = 0.0f;
                    _move.ApplyFriction(dt, _tune);
                    _move.WalkMove(wishDir, wishSpeed, dt, _tune);
                }
                else
                {
                    _move.AirMove(wishDir, wishSpeed, dt, _tune);
                }
                _move.ClampVelocity(_tune);

                float ny = gamePos.Y;
                if (wallMove)
                {
                    // Wall motion owns vertical this tick. Anchoring at
                    // _lastWritten discards the native jump impulse so Space
                    // does not fight the climb/run.
                    ny = _lastWritten.Y + wallVelocity.Y * dt * _worldScale;
                }
                else if (_hlVertical && _hlJumpActive) {
                    // Large native corrections cannot be identified as gravity.
                    // Yield rather than restore an obsolete absolute jump pose.
                    if (Math.Abs(ey) > 16 * _worldScale) { ResetVertical(); _nativeVerticalActive = true; }
                    else {
                        float verticalStep = HlJump.Integrate(ref _jumpVel, _tune.Gravity, dt);
                        _jumpVel = Clamp(_jumpVel, -_tune.MaxVelocity, _tune.MaxVelocity);
                        _jumpCarry += verticalStep;
                        ny = (jumped ? gamePos.Y : _lastWritten.Y) + verticalStep * _worldScale;
                    }
                }
                else if (!_hlVertical) { ResetVertical(); }

                Vec3 delta = HlCollision.RemainingHorizontal(_move.Velocity * (dt * _worldScale),
                                                            gamePos - _lastWritten, _replaceGameMovement);
                float blockBelow = wallMove ? HlCollision.WalkableNormalY : HlCollision.SteepNormalY;
                if (!_collision.TryHorizontal(_playerAddr, gamePos, delta, _worldScale, ref _move.Velocity,
                                               out Vec3 horizontalPos, out _hasWall, out _wallNormal, blockBelow)) {
                    ModLog.Warn("body query failed/invalid; stopped without a position write"); Deactivate(); return;
                }

                // Follow walkable slopes, platform edges and the lower half-pipe
                // within one step instead of leaving support and falling through
                // the horizontal sweep. Wall mode already owns vertical motion.
                if (!wallMove && !jumped && !_hlJumpActive && !_nativeVerticalActive)
                {
                    if (_collision.TryStepSupport(_playerAddr, horizontalPos, _worldScale,
                                                  out bool stepSupported, out float stepY, out Vec3 stepNormal))
                    {
                        float stepGap = gamePos.Y - stepY;
                        bool into = Vec3.Dot(wishDir, new Vec3(-stepNormal.X, 0.0f, -stepNormal.Z)) > 0.05f;
                        if (stepSupported && Finite(stepY) &&
                            stepGap <= HlCollision.StepHeight * _worldScale &&
                            stepGap >= -HlCollision.StepHeight * _worldScale &&
                            (onGround || (_grounded && into)))
                            ny = stepY;
                    }
                }

                if (!_collision.TryVertical(_playerAddr, horizontalPos, ny, _worldScale, out ny, out bool landed, out bool ceiling)) {
                    ModLog.Warn("vertical query failed/invalid; stopped without a position write"); Deactivate(); return;
                }
                if (landed) { ResetVertical(); _wall.Reset(); wallMove = false; onGround = true; }
                else if (ceiling) { _jumpVel = Math.Min(0, _jumpVel); }
                // Recheck the final support, never a pre-movement/stale floor.
                var finalPos = new Vec3(horizontalPos.X, ny, horizontalPos.Z);
                bool finalSupport = ProbeGround(finalPos, out float finalGroundY, out Vec3 _);
                if (!_supportQueryValid) { ModLog.Warn("final support query failed/invalid; stopped without a position write"); Deactivate(); return; }
                bool finalSteep = false;
                if (!wallMove &&
                    _collision.TryStepSupport(_playerAddr, finalPos, _worldScale,
                                              out bool finalStepSupported, out float finalStepY, out Vec3 _) &&
                    finalStepSupported && Math.Abs(finalPos.Y - finalStepY) <= HlCollision.Skin * _worldScale)
                    finalSteep = true;
                onGround = !wallMove && !_hlJumpActive && !_nativeVerticalActive && !_wall.Active &&
                           ((finalSupport && finalPos.Y-finalGroundY <= HlCollision.Skin*_worldScale) || finalSteep);

                if (Game.WritePlayerPosition(finalPos.X, finalPos.Y, finalPos.Z))
                {
                    _lastWritten = finalPos;
                    _speed = _move.Speed;
                    gamePos = finalPos;   // camera follows the written position
                }
                else
                {
                    ModLog.Warn("failed to write player position (strat disappeared?)");
                    Deactivate();
                    return;
                }
            }
            else
            {
                _speed = 0.0f;
                _lastWritten = gamePos;
                _move.Reset();
                ResetVertical();
                _wall.Reset();
            }

            _lastGame = new Vec3(px, py, pz);
            _grounded = onGround;
            Game.SetNinjaFacing(_yaw);

            // Telemetry ring for the speed plot in the UI.
            for (int i = 0; i < _speedHistory.Length - 1; i++)
                _speedHistory[i] = _speedHistory[i + 1];
            _speedHistory[_speedHistory.Length - 1] = _speed;

            if (_cameraEnabled)
                ApplyCamera(gamePos);
            if (_active && !Input.LeaseMovementKeys(_playerAddr, _movementEnabled && _replaceGameMovement)) {
                ModLog.Warn("native WASD ownership unavailable; stopped instead of mixing movement directions");
                Deactivate();
            }
        }

        private void HandleHotkeys()
        {
            bool overlay = Input.OverlayOpen;

            bool f2 = Input.KeyDown(Keys.F2);
            if (f2 && !_f2Held && !overlay)
            {
                if (_active) Deactivate();
                else Activate();
            }
            _f2Held = f2;

            bool f3 = Input.KeyDown(Keys.F3);
            if (f3 && !_f3Held && _active)
                Deactivate();
            _f3Held = f3;
        }

        private void UpdateMouseLook()
        {
            Input.GetMouseDelta(out float dx, out float dy);
            if (dx == 0.0f && dy == 0.0f)
                return;

            if (Input.CaptureActive)
            {
                float sens = _sensitivity * DegToRad;
                _yaw += dx * sens;
                _pitch += (_invertY ? dy : -dy) * sens;
                _pitch = Clamp(_pitch, -89.0f * DegToRad, 89.0f * DegToRad);
            }
            Input.ConsumeMouseDelta();
        }

        private Vec3 BuildWishVelocity()
        {
            float forward = (Input.KeyDown(Keys.W) ? 1.0f : 0.0f) - (Input.KeyDown(Keys.S) ? 1.0f : 0.0f);
            float side    = (Input.KeyDown(Keys.D) ? 1.0f : 0.0f) - (Input.KeyDown(Keys.A) ? 1.0f : 0.0f);
            if (forward == 0.0f && side == 0.0f)
                return Vec3.Zero;

            float sy = (float)Math.Sin(_yaw);
            float cy = (float)Math.Cos(_yaw);

            // +Z forward at yaw 0, +X right; same basis as HlCamera.MakeWorld.
            var fwd   = new Vec3(sy, 0.0f, cy);
            var right = new Vec3(cy, 0.0f, -sy);

            // HL command values are in u/s; PM_CheckParamters clamps the
            // resulting wish speed to maxspeed, which the caller does.
            return fwd * (forward * _tune.MaxSpeed) + right * (side * _tune.MaxSpeed);
        }

        private void ApplyCamera(Vec3 playerPos)
        {
            float sy = (float)Math.Sin(_yaw);
            float cy = (float)Math.Cos(_yaw);

            var eye = new Vec3(
                playerPos.X + sy * _forwardOffset * _worldScale,
                playerPos.Y + Math.Max(1.0f, _eyeHeight) * _worldScale,
                playerPos.Z + cy * _forwardOffset * _worldScale);

            HlCamera.MakeWorld(eye, _yaw, _pitch, 0.0f, _camMatrix);
            if (!Game.SetCameraWorld(_camMatrix))
            {
                ModLog.Warn("camera write failed; stopped");
                Deactivate();
                return;
            }

            // Camera scripts may restore their own FOV; keep ours applied.
            if (Math.Abs(Game.FovDegrees - _fov) > 0.05f)
            {
                Game.FovDegrees = _fov;
                _fovOwned = true;
            }
        }

        public override void OnOverlayChanged(bool open)
        {
            if (open)
            {
                Input.LeaseMovementKeys(0, false);
                _move.Reset(); ResetVertical(); _wall.Reset(); _spaceHeld=Input.KeyDown(Keys.Space); _jumpHeld=Input.KeyDown(_jumpKey);
                Input.ConsumeMouseDelta();
                if (_captureOwned)
                {
                    Input.SetCapture(false);
                    _captureOwned = false;
                }
                FlushSettings();
            }
            else if (_active && _cameraEnabled && _captureMouse)
            {
                EnsureCapture(true);
                Input.ConsumeMouseDelta();
            }
            if (!open) FlushSettings();
        }

        public override void OnEnabledChanged(bool enabled)
        {
            if (!enabled) Deactivate();
        }

        // =====================================================================
        //  UI
        // =====================================================================
        public override void OnGui()
        {
            float s = Math.Max(1.0f, ImGui.Scale);
            float width = 230.0f * s;

            // ---------------------------------------------------------- status
            ImGui.TextColored(_active ? ColOk : ColDim,
                _active ? "ACTIVE" : "STANDBY");
            ImGui.SameLine();
            uint playerColor = Game.PlayerValid ? ColOk : ColWarn;
            string playerText = Game.PlayerValid
                ? "player 0x" + Game.PlayerAddr.ToString("X8")
                : "no player tracked";
            ImGui.TextColored(playerColor, playerText);
            ImGui.SameLine();
            ImGui.TextDisabled("| F2 toggle, F3 panic");
            ImGui.TextColored(ColWarn, "Experimental post-tick adapter. Pin exact class Ninja before activation.");

            if (!_active)
            {
                if (ImGui.Button("Activate (F2)", 150.0f * s))
                    Activate();
                ImGui.SameLine();
            }
            else
            {
                if (ImGui.Button("Deactivate (F3)", 150.0f * s))
                    Deactivate();
                ImGui.SameLine();
            }
            if (ImGui.Button("Resync state", 120.0f * s))
            {
                ResyncToPlayer();
                Input.ConsumeMouseDelta();
                ModLog.Info("state resynced to the game's player position");
            }
            ImGui.SameLine();
            if (ImGui.Button("HL1 defaults", 120.0f * s))
            {
                _tune.Gravity = 800.0f;
                _tune.MaxSpeed = 320.0f;
                _tune.Accelerate = 10.0f;
                _tune.AirAccelerate = 10.0f;
                _tune.Friction = 4.0f;
                _tune.StopSpeed = 100.0f;
                _tune.JumpSpeed = 268.328f;
                _tune.MaxVelocity = 2000.0f;
                _tune.GameVerticalThreshold = 180.0f;
                Touch();
            }

            // --------------------------------------------------------- session
            ImGui.SeparatorText("Session");

            if (ImGui.Checkbox("Enable movement physics", ref _movementEnabled))
            {
                ResyncToPlayer();
                Touch();
            }
            ImGui.SameLine();
            if (ImGui.Checkbox("Enable FPS camera", ref _cameraEnabled))
            {
                if (_active) Game.SetExternalCameraOwned(_cameraEnabled);
                EnsureCapture(false);
                if (!_cameraEnabled && _active && _fovOwned && _savedFovDegrees >= 1.0f)
                {
                    Game.FovDegrees = _savedFovDegrees;
                    _fovOwned = false;
                }
                Touch();
            }

            if (ImGui.Checkbox("Replace game movement", ref _replaceGameMovement))
                Touch();
            if (ImGui.IsItemHovered())
                ImGui.SetTooltip("On: credit native movement already made along the HL direction.\n" +
                                 "Native corrections/platform motion are never rewound.\n" +
                                 "Off: adds HL velocity on top of the game's own movement\n" +
                                 "(double speed if the game also walks the ninja).");

            if (ImGui.Checkbox("HL vertical (jump + gravity)", ref _hlVertical))
            {
                ResyncToPlayer();
                Touch();
            }
            if (ImGui.IsItemHovered())
                ImGui.SetTooltip("On: collision-driven HL flight; native takeoffs have priority.\n" +
                                 "Off: vertical movement stays entirely with I-Ninja; only the\n" +
                                 "horizontal pmove is applied.");

            if (ImGui.Checkbox("Auto-hop", ref _autoHop)) Touch();
            ImGui.SameLine();
            if (ImGui.Checkbox("Bunny speed cap", ref _capBunnySpeed)) Touch();

            int keyIndex = Array.IndexOf(JumpKeyCodes, _jumpKey);
            if (keyIndex < 0) keyIndex = 0;
            ImGui.SetNextItemWidth(120.0f * s);
            if (ImGui.Combo("Mod jump key", ref keyIndex, JumpKeyNames))
            {
                _jumpKey = JumpKeyCodes[keyIndex];
                _jumpHeld = Input.KeyDown(_jumpKey);
                Touch();
            }
            if (ImGui.IsItemHovered())
                ImGui.SetTooltip("Space stays an I-Ninja wall action: first press at a wall climbs or\n" +
                                 "runs, second press kicks away. This mod jump key is only for\n" +
                                 "ground takeoffs with the HL impulse (default V).");

            // --------------------------------------------------------- physics
            ImGui.SeparatorText("Physics");
            ImGui.TextDisabled("HLSDK single-player defaults: 800 gravity, 320 maxspeed, 10/10 accel, 4 friction");

            float scaleBefore = _worldScale;
            Slider("World units / HL unit", ref _worldScale, 0.001f, 0.100f, "%.4f", width);
            if (scaleBefore != _worldScale) ResyncToPlayer();
            ImGui.TextColored(ColWarn, "Scale is an estimate; confirm the player before calibrating.");
            ImGui.TextDisabled(string.Format("320 HL u/s = {0:F2} game u/s; eye {1:F2}; jump apex about {2:F2} game units",
                320.0f * _worldScale, _eyeHeight * _worldScale,
                _tune.Gravity > 0 ? _tune.JumpSpeed * _tune.JumpSpeed / (2 * _tune.Gravity) * _worldScale : 0));

            Slider("Max speed", ref _tune.MaxSpeed, 60.0f, 1200.0f, "%.0f u/s", width);
            Slider("Accelerate", ref _tune.Accelerate, 1.0f, 40.0f, "%.1f", width);
            Slider("Air accelerate", ref _tune.AirAccelerate, 1.0f, 40.0f, "%.1f", width);
            Slider("Friction", ref _tune.Friction, 0.0f, 20.0f, "%.1f", width);
            Slider("Stop speed", ref _tune.StopSpeed, 0.0f, 400.0f, "%.0f u/s", width);
            Slider("Gravity", ref _tune.Gravity, 0.0f, 2400.0f, "%.0f u/s2", width);
            Slider("Jump speed", ref _tune.JumpSpeed, 0.0f, 600.0f, "%.1f u/s", width);
            Slider("Native fall threshold", ref _tune.GameVerticalThreshold, 20.0f, 600.0f, "%.0f u/s", width);
            if (ImGui.IsItemHovered())
                ImGui.SetTooltip("Downward native speed cutoff while near a support surface.\n" +
                                 "Rising native takeoffs always have priority; nearby support\n" +
                                 "alone cannot grant jump permission. Stored under the existing setting key.");

            // ---------------------------------------------------------- camera
            ImGui.SeparatorText("FPS camera");

            if (ImGui.Checkbox("Capture mouse", ref _captureMouse))
            {
                Touch();
                if (!_captureMouse && _captureOwned)
                {
                    Input.SetCapture(false);
                    _captureOwned = false;
                }
            }
            ImGui.SameLine();
            if (ImGui.Checkbox("Invert Y", ref _invertY)) Touch();

            Slider("Sensitivity", ref _sensitivity, 0.002f, 0.200f, "%.3f deg/count", width);
            Slider("Eye height", ref _eyeHeight, 0.0f, 300.0f, "%.0f HL units", width);
            Slider("Forward offset", ref _forwardOffset, 0.0f, 60.0f, "%.0f HL units", width);
            Slider("Field of view", ref _fov, 50.0f, 130.0f, "%.1f deg", width);

            if (ImGui.Button("Eye height from game camera", 240.0f * s))
            {
                if (Game.TryGetPlayerPosition(out float _, out float eyeBaseY, out float __) &&
                    Game.TryReadCamera(out CameraInfo camNow))
                {
                    _eyeHeight = Clamp((camNow.Y - eyeBaseY) / _worldScale, 2.0f, 300.0f);
                    Touch();
                }
            }
            ImGui.SameLine();
            if (ImGui.Button("Aim camera at player", 180.0f * s))
            {
                InitLookFromCamera();
                Input.ConsumeMouseDelta();
            }

            // --------------------------------------------------------- player
            ImGui.SeparatorText("Player tracking");

            int mode = Game.PlayerMode;
            string modeText = mode == 0 ? "auto" : mode == 1 ? "manual" : "learning";
            ImGui.TextDisabled("tracker mode: " + modeText + "   candidates: " + Game.PlayerCandidateCount);
            ImGui.Text("Class: " + (Game.PlayerPinned ? "Ninja (pinned/validated)" : "<not validated>"));
            ImGui.PushTextWrapPos();
            ImGui.Text("Attachments, camera, speech and unknown identities are refused. Exact identity does not yet verify collision or whole-body synchronization.");
            ImGui.PopTextWrapPos();

            if (ImGui.Button("Auto-detect", 120.0f * s))
            {
                Deactivate();
                Game.PlayerSetMode(0);
                ResyncToPlayer();
            }
            ImGui.SameLine();
            if (ImGui.Button("Learn candidates", 180.0f * s))
            {
                Deactivate();
                Game.PlayerLearn();
                ModLog.Info("learning started: close F9, stand still, move with WASD/arrows, stop; then confirm a candidate in Inspector > Player");
            }

            ImGui.SetNextItemWidth(120.0f * s);
            ImGui.InputText("Strat hex##playeraddr", ref _playerAddrText, 16);
            ImGui.SameLine();
            if (ImGui.Button("Select##playeraddr", 80.0f * s))
            {
                if (uint.TryParse(_playerAddrText, NumberStyles.HexNumber, CultureInfo.InvariantCulture, out uint selected))
                {
                    Deactivate();
                    Game.PlayerSelect(selected);
                    ResyncToPlayer();
                    ModLog.Info("manual player strat 0x" + selected.ToString("X8"));
                }
                else
                {
                    ModLog.Warn("invalid hex address");
                }
            }

            // ------------------------------------------------------ diagnostics
            ImGui.SeparatorText("Diagnostics");

            if (Game.TryGetPlayerPosition(out float px, out float py, out float pz))
            {
                ImGui.Text("pos      " + Fmt(px, py, pz));
                ImGui.Text("hl vel   " + _move.Velocity);
                ImGui.SameLine();
                ImGui.TextDisabled(string.Format("  {0:F1} u/s", _speed));
                ImGui.Text(string.Format("game dy  {0:+0.000;-0.000;0.000} game u/tick   ({1:F0} HL u/s)", _gameVy * Game.TimeStep * _worldScale, _gameVy));
                ImGui.Text(string.Format("state    {0}   flight {1}   wall {2} {3}   offset {4:F1}   jumpVel {5:F1}",
                    _grounded ? "GROUND" : "AIR",
                    _hlJumpActive ? "HL" : _nativeVerticalActive ? "native" : "none",
                    _wall.Mode == WallMode.None ? "none" : _wall.Mode.ToString().ToLowerInvariant(),
                    _wall.Active ? _wall.TicksRemaining.ToString() : "-",
                    _jumpCarry, _jumpVel));
            }
            else
            {
                ImGui.TextColored(ColWarn, "player position unavailable");
            }

            ImGui.Text(string.Format("yaw {0:F1} deg   pitch {1:F1} deg   fov {2:F0}",
                _yaw * RadToDeg, _pitch * RadToDeg, _fov));
            ImGui.Text(string.Format("capture {0}   owned {1}   overlay {2}",
                Input.CaptureActive ? "on" : "off",
                _captureOwned ? "yes" : "no",
                Input.OverlayOpen ? "open" : "closed"));
            ImGui.TextDisabled(string.Format("tick {0}  dt {1:F4}s  tickrate {2:F0} Hz",
                Game.TickCount, Game.TimeStep, Game.TimeStep > 0.0f ? 1.0f / Game.TimeStep : 0.0f));

            if (!Game.IsReady)
                ImGui.TextColored(ColBad, "Game is not ready (no level loaded).");
            else if (!Game.PlayerValid)
                ImGui.TextColored(ColWarn, "No player tracked: use Auto-detect, Learn, or the Trainer's Player page.");

            ImGui.PlotLines("Speed##hl_speed_plot", _speedHistory, 60.0f * s);

            ImGui.TextDisabled("HLSDK movement port: pm_shared.c (PM_Friction / PM_Accelerate /");
            ImGui.TextDisabled("PM_AirAccelerate / PM_WalkMove / PM_AirMove / PM_Jump).");
            ImGui.TextDisabled("I-Ninja walls: Space at contact = climb/run, Space again = kick.");
            ImGui.TextDisabled("Steep half-pipes and slopes are followed with bounded step support.");
        }

        private void Slider(string label, ref float value, float min, float max, string format, float width)
        {
            ImGui.SetNextItemWidth(width);
            if (ImGui.SliderFloat(label, ref value, min, max, format))
                Touch();
        }

        private static float Clamp(float v, float lo, float hi) => v < lo ? lo : (v > hi ? hi : v);
        private static bool Finite(float v) => !float.IsNaN(v) && !float.IsInfinity(v);
        private static float Bound(float v, float lo, float hi, float fallback)
            => Finite(v) ? Clamp(v, lo, hi) : fallback;

        private static string Fmt(float x, float y, float z)
            => string.Format("{0,9:F2}, {1,9:F2}, {2,9:F2}", x, y, z);
    }
}
