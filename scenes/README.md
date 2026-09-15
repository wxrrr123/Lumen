# Scenes

Lumen 支援兩種場景格式：

- **Lumen JSON**（`cornell_box/*.json`、`caustics.json`、`occluded*.json` …）：原作者的格式，每個 integrator 一個檔。
- **Mitsuba 0.5 XML**（`classroom/`、`bedroom/`、`kitchen/` …）：[Benedikt Bitterli 的場景庫](https://benedikt-bitterli.me/resources/)
  裡的 `scene_v0.6.xml`，由 `src/Framework/MitsubaParser.cpp` + `LumenScene::load_mitsuba_scene` 直接讀取，
  **不需要手動改 xml**（相機、燈光、材質的轉換都在 loader 裡）。

## 新增一個 Bitterli 場景

1. 到 <https://benedikt-bitterli.me/resources/> 下載該場景的 **Mitsuba** 版 zip（`mitsuba/<name>.zip`）。
2. 執行：

   ```bash
   python3 scenes/prepare_mitsuba_scene.py ~/Downloads/<name>.zip            # 預設 restirpt / maxDepth 10
   python3 scenes/prepare_mitsuba_scene.py ~/Downloads/<name>.zip --integrator path --depth 17
   ```

   會解壓到 `scenes/<name>/`、從 `scene_v0.6.xml` 產生 `scene.xml`（只換 integrator 與 maxDepth），
   並列出這個場景裡 loader 不支援的東西。`models/`、`textures/` 已在 `.gitignore`，只有 xml 會進 git。
3. 跑：

   ```bash
   # 互動
   ./build_small_buf/Lumen scenes/<name>/scene.xml
   # headless（遠端），解析度建議用 xml 裡 film 的 width/height，才會跟參考圖同構圖
   ./build_small_buf/Lumen scenes/<name>/scene.xml --headless --frames 30 --width 1280 --height 720 --output out.exr
   ```

   `TungstenRender.png` / `.exr` 是 Bitterli 附的參考圖（Tungsten 渲染），可以直接拿來對照。

## Loader 支援範圍（Mitsuba 0.5 XML）

| 類別 | 支援 | 不支援 / 近似 |
|---|---|---|
| Shape | `obj`、`rectangle`、`disk`、`cube`、`sphere`（後四者程式產生三角形） | `hair`、`ply`、`serialized` → 跳過並警告 |
| Emitter | shape 內的 `area`（radiance）、`sunsky`/`sun`（含 `scale`/`sunScale`）/`directional`、`constant` | `envmap` → 用整張圖的平均色當常數天空（**近似**，沒有方向性、鏡面反射會錯） |
| BSDF | `diffuse`、`roughdiffuse`、`plastic`、`roughplastic`、`dielectric`、`roughdielectric`、`thindielectric`、`conductor`、`roughconductor`、`glass`；包裝層 `twosided`、`mask`、`bumpmap`、`normalmap`、`coating` 會被解開 | `mask` 的 **opacity 被忽略**（紗簾、百葉窗變不透明）；bump/normal map 忽略；其它型別退化成 diffuse |
| Texture | `bitmap`（每張檔案只載一次） | `checkerboard` → 用 color0/color1 平均色；其它 procedural 忽略 |
| Sensor | `fov` + `fovAxis`（換算成垂直 fov）、`toWorld`、film 解析度 | 景深、其它相機型別 |
| 其它 | | participating media 忽略 |

幾個 loader 的慣例：

- **相機**：Mitsuba 相機空間是 +x 左、+y 上、看向 +z；Lumen 是 +x 右、+y 上、看向 −z。loader 會把 camera-to-world
  矩陣的第 0、2 欄取負，並用 YXZ 順序抽 Euler 角（對應 `Camera::update_view_matrix`）。
- **Area light 只有正面發光**：Mitsuba 的 area emitter 是單面的，Lumen 的是雙面。loader 會在 `rectangle`/`disk`
  發光體背後 1 mm 處加一片黑色背板（`__emitter_backing_*` 材質），把背面擋掉。
- **sunsky**：原始檔只有 `turbidity`/`sunScale`/`skyScale`，loader 用固定顏色：太陽 `(1, 0.95, 0.85) × sunScale`、
  天空 `(0.53, 0.8, 0.92) × skyScale × 0.25`。若 xml 有 Lumen 式的 `sunColor`/`skyColor`（原作者手改的舊格式），則沿用
  舊行為（`100 × sunColor × sunScale`）。
- 沒有任何光源的場景（例如 envmap 被跳過）仍會建立 lights buffer，不會 crash，只是全黑。

## 目前狀態（ReSTIR PT, 30 frames, 與 Tungsten 參考圖比較）

| 場景 | 狀態 | 亮度比 (mean / median) | 備註 |
|---|---|---|---|
| `cornell-box` | ✅ | 1.01 / 1.00 | 純 diffuse，校正用 |
| `classroom` | ✅ | 1.02 / 0.98 | sunsky 用預設顏色 |
| `bathroom` | ✅ | 0.99 / 0.80 | 百葉窗／地毯 `mask` 忽略 |
| `kitchen` | ✅ | 0.97 / 0.91 | 窗簾 `mask` 忽略 |
| `staircase` | ✅ | 0.78 / 0.61 | 直式 720×1280，bumpmap 忽略；wood 是 roughplastic，比參考略暗 |
| `dining-room` | ✅ | 1.42 / 1.25 | sunsky 預設顏色 |
| `veach-ajar` | ✅ | 1.05 / 1.03 | 地板 checkerboard 用平均色 |
| `bathroom2` | ✅ | 1.05 / 0.91 |  |
| `staircase2` | ✅ | 0.93 / 0.84 | 13 盞小燈（disk + rectangle） |
| `veach-bidir` | ✅ | 1.14 / 1.12 |  |
| `veach-mis` | ✅ | 1.17 / 1.18 | 3 顆 sphere 燈 |
| `spaceship` | ✅ | 1.55 / 1.00 | 4 盞 area light + `constant` 天空 |
| `dragon` | ✅ | 0.93 / 0.00 | 只有 `sun`，黑背景 |
| `bedroom` | 🔶 | 0.16 / 0.14 | 窗簾是 `mask`（opacity 0.53）→ 目前不透明，把窗光擋掉，整體偏暗 |
| `house` | 🔶 | 1.61 / 1.63 | sunsky 預設顏色，偏亮 1.6×；只有陽光＋常數天空 |
| `living-room-2` | 🔶 | 0.82 / 0.59 | 2 個 `mask` 忽略；沙發 plastic 偏暗 |
| `living-room-3` | 🔶 | 0.54 / 0.52 | 燈在 sphere 上（已支援）；`hair` 地毯跳過 |
| `lamp` | 🔶 | 0.95 / 0.70 | 燈罩內 area light 對；背景光在原場景來自超大半徑的 `sun`，我們是方向光，背景偏暗 |
| `coffee` | 🔶 | 0.62 / 0.47 | 3 個小 softbox；整體比參考暗 |
| `glass-of-water` | 🔶 | 1.80 / 1.90 | 玻璃／水正確，背板偏亮 |
| `living-room` | 🔶 | 0.38 / 0.51 | envmap 用平均色近似 |
| `car` | 🔶 | 1.15 / 0.77 | envmap 用平均色近似；車漆 `coating` 只取內層，顏色不對 |
| `car2` | 🔶 | 1.18 / 0.57 | 同 car |
| `teapot` | 🔶 | 0.88 / 0.86 | envmap 用平均色近似 |
| `teapot-full` | 🔶 | 1.19 / 1.17 | envmap 近似；茶水 medium 忽略 |
| `water-caustic` | ❌ | 0.00 / 0.37 | 純焦散場景（radiance 5e5 的小燈打進水體），水面全黑；ReSTIR PT 本來就不適合 |

✅ 構圖、光照與參考圖一致　🔶 可用但有已知近似　❌ 目前不可用

其餘 Bitterli 場景：`lego`、`matpreview`、`rover` 只有 Mitsuba 3 格式（`.serialized`/ply + envmap）；`curly-hair`、`furball`、`hair-curl`、`straight-hair` 需要 `hair` shape；`volumetric-caustic` 需要 media；`material-testball` 只有 envmap。都沒做。

**ReSTIR PT vs Path**：同一個場景描述下，所有 area-light 場景的 ReSTIR PT / Path 平均亮度比都在 0.98–1.00（cornell-box、staircase2、veach-ajar、veach-mis、spaceship、coffee、glass-of-water、living-room-2…）。有 `sun`/`sunsky` 的場景（classroom、house、dining-room、dragon、lamp）兩者會差，原因是 `path.rgen` 在 miss 時固定走 `shade_atmosphere()`（大氣散射模型），而 `gris.rgen` 預設用常數 `sky_col`（`ReSTIRPT::enable_atmosphere = false`）；把 ReSTIR PT 的 atmosphere 打開後兩者就一致（classroom 0.98、house 1.00、dining-room 0.96）。論文的 ground truth 請用 Lumen 自己的 Path 收斂結果，Tungsten 參考圖只用來檢查 loader。

「亮度比」是線性 EXR 平均值 ours / TungstenRender.exr；大於 1 代表比參考亮。主要偏差來源是 `mask` 不透明（擋光的紗簾／
百葉窗）、envmap 近似、以及 sunsky 的固定顏色。

## 校正場景

`calibration/furnace.xml`：albedo 0.5 的地板 + 100 m 見方、radiance 1 的發光天花板（高度 1 m），相機往下看。
解析解是地板 radiance = 0.5；Path 和 ReSTIR PT 都得到 0.4998。這個測試主要走 BSDF sampling（燈太大，NEE 的 MIS 權重很小）。

`calibration/small_light.xml`：同上，但燈縮成 1 m × 1 m。畫面中央的解析解是 ρ·E/π = 0.5 × 0.7523 / π = **0.1197**
（E 是點正下方對 1×1 Lambertian 面光源的 irradiance）。這個測試由 NEE 主導，能抓到光源取樣 pdf 的錯誤：
修掉 `sample_triangle` 對邊向量／法線用 w=1 做 world transform 的 bug 之前，這裡會得到 0.162（1.35 倍）；修掉之後
Path 0.1195、ReSTIR PT 0.1198。

```bash
./build_small_buf/Lumen scenes/calibration/furnace.xml     --headless --frames 50 --width 256 --height 256 --output furnace.exr
./build_small_buf/Lumen scenes/calibration/small_light.xml --headless --frames 50 --width 256 --height 256 --output small.exr
```

`cornell-box/`（Bitterli 版）也可以當校正用：純 diffuse、一盞 area light，修正後 ours/ref = 1.01。

這兩個測試抓到的上游 Lumen bug（都在 `src/shaders/commons.glsl`，identity world matrix、燈的三角形數相同時看不出來）：

1. `sample_triangle()` 用 `vec4(edge, 1.0)` 轉換邊向量和法線，把平移量混進光源取樣的 pdf。
2. 選燈用「每盞燈等機率」（`rands.x * num_lights`），但 integrator 除的是 `1 / light_triangle_count`（每個三角形等機率）；
   燈的三角形數量不同時（lamp、spaceship、staircase2）偏差可達數百倍。現在由 `pick_light_triangle()` 依三角形數量成比例選燈。
