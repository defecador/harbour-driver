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
