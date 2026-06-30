"""PlatformIO pre-build hook: inject .env values as -D build flags.

Keeps WiFi creds + location out of source. String values (SSID/PW/TZ) are
emitted as properly-escaped C string literals; LAT/LON go through as numeric
literals. Missing .env is non-fatal — main.cpp has fallback #defines.
"""
Import("env")
import os

STRINGS = ("WIFI_SSID", "WIFI_PW", "TZ")  # need C-string quoting
env_path = os.path.join(env.subst("$PROJECT_DIR"), ".env")

if os.path.exists(env_path):
    with open(env_path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#") or "=" not in line:
                continue
            key, val = line.split("=", 1)
            key = key.strip()
            val = val.strip().strip('"').strip("'")
            if key in STRINGS:
                env.Append(CPPDEFINES=[(key, env.StringifyMacro(val))])
            else:
                env.Append(CPPDEFINES=[(key, val)])
else:
    print("load_env.py: no .env found — using firmware fallbacks (daytime only)")
