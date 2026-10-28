# Driver

Driver is an independent, unofficial trip planner for
Sailfish OS, in the spirit of Google's HERE WeGo: plan trips by car (with
live traffic), public transport, bike and on foot.

Driver is not affiliated with or endorsed by HERE Global B.V. It uses HERE's
routing, transit, search and map APIs, but ships no HERE branding, icons or
artwork of its own.

## Status

Milestone 1: trip planning end to end (search, directions, route and transit
details). Turn-by-turn navigation is not implemented yet.

## Getting a HERE API key

Driver needs your own [HERE](https://developer.here.com) API key to fetch
routes, transit itineraries, search results and map tiles:

1. Create a free HERE Developer account.
2. Create a REST API key (the free tier is generous enough for personal use).
3. Open Driver → Settings → paste the key → **Test key**.

Without a key, Driver still runs: the map falls back to
[OpenFreeMap](https://openfreemap.org) / OpenStreetMap, but search, routing
and transit are unavailable.

## Building

Requires the [Sailfish OS SDK](https://sailfishos.org/wiki/Application_SDK)
and, on the target device, the
[`mapboxgl-qml`](https://github.com/rinigus/mapbox-gl-qml) package from
[Chum](https://openrepos.net/content/chum). Without it, Driver still starts
and shows a "map unavailable" placeholder.

```bash
sfdk -c target=SailfishOS-5.1.0.11EA-aarch64 build
sfdk -c target=SailfishOS-5.1.0.11EA-aarch64 deploy --sdk --all
```

### Android (tablet and phone)

The same source builds as an Android app with Qt 6.7 and
[MapLibre Native Qt](https://github.com/maplibre/maplibre-native-qt) (the
Sailfish package above is unaffected). The window is resizable, so it works in
split screen: wide landscape windows show a left panel beside the map, narrow
ones use the phone layout.

Needs: JDK 17, Android SDK (platform 34, build-tools 34, NDK 26.1.10909125),
Qt 6.7.3 for `gcc_64` and Android (`aqtinstall`, with the `qtpositioning`,
`qtlocation`, `qt5compat` and `qtshadertools` modules), the prebuilt MapLibre
Native Qt 3.0.0 for Qt 6.7.3 Android, and
[KDAB/android_openssl](https://github.com/KDAB/android_openssl). `tools/android-env.sh`
shows the paths it expects.

```bash
. tools/android-env.sh
~/Qt/6.7.3/android_arm64_v8a/bin/qt-cmake -S . -B build-android -DQT_HOST_PATH=$HOME/Qt/6.7.3/gcc_64 \
    -DCMAKE_BUILD_TYPE=Release -DQT_ANDROID_SIGN_APK=ON    # needs QT_ANDROID_KEYSTORE_* set
cmake --build build-android --target apk
```

Use `android_x86_64` instead of `android_arm64_v8a` (and no signing) for an
emulator build. The QML for Qt 6 is generated from `qml/` by
`tools/gen_qt6_qml.py`, with Android-only replacements in `android/qml/`
(map, window, keep-awake/vibration/notifications). On Android the HERE key
lives in `files/settings/settings.conf` inside the app's data folder.

### Developer flags

- `--fixtures` — serve HERE API responses from local JSON files in
  `fixtures/` instead of the network, so the UI can be exercised without a
  key or connectivity.
- `--dev-screen=<name>` — open directly on a given screen with sample data.
- `--dev-grab=<path.png>` — render once, save a screenshot, and quit.
- `--dev-dark` — force the dark theme regardless of the ambience.

## License

GPL-3.0-or-later. See [LICENSE](LICENSE).

Fonts and icon font ship under their own licenses — see
`assets/fonts/OFL.txt` and `assets/fonts/LICENSE-MaterialIcons.txt`.

Routing, transit, search and map data © HERE, subject to
[HERE's End User Terms](https://legal.here.com/en-gb/terms) and
[Privacy Policy](https://legal.here.com/en-gb/privacy).
