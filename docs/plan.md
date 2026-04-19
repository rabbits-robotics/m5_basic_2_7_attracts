# m5_basic_2_7_attracts — Bluetooth ゲームパッド実装

## Context

`attracts_ws` にある **`gamepad_node`** と **`transceiver_module_bridge_node`** 相当の機能を M5Stack Basic v2.7（ESP32）で実装する。M5Stack のボタンで 2 モードを切り替えられるようにし、**まず gamepad モードから実装する**（transceiver モードは空スタブ）。

- ゲームパッドは **Bluetooth HID**（Bluepad32）で接続
- 生成した `AttractsCommand` は **UART バイナリ** で STM32（`beta_main` / rabcl プロトコル）に送る
- ビルドは **PlatformIO**（beta_main と同じ方式）

これにより、PC + ROS2 を介さずに M5Stack 単独で beta_main を操縦できる。

**実装順序の方針**: まず **内部状態構造体 (State) と LCD 描画** を先に作って画面で動作を確認できる土台を作り、その後に UART 送信、最後にゲームパッド入力を載せる。こうすると各段階でデバッグ対象が 1 つずつ増えるだけなので切り分けやすい。

---

## 設計概要

### ハード接続（ピン割当て）

| 役割 | ポート | M5Stack ピン | プロトコル | 方向 | 今回使用 |
|---|---|---|---|---|---|
| STM32 ブリッジ | Port C (UART2 / `Serial2`) | GPIO16=RX, GPIO17=TX | 230400 bps 8N1 | M5 ↔ STM32（まず TX のみ使用） | ◎ 実装 |
| トランシーバ受信 | Port B (UART1 / `Serial1`) | **GPIO36=RX**, GPIO26=TX 予約 | 115200 bps 8N1 | トランシーバ → M5（RX のみ） | ◯ ピン確保のみ |
| ゲームパッド | Bluetooth HID | — | Bluepad32 | — | ◎ 実装 |

**理由**:
- Port C (GPIO16/17) は UART2 の既定ピンで双方向使えるので、将来 STM32 Feedback（112B）も受けられるよう残す
- Port B の GPIO36 は **入力専用ピン**なので RX に最適。トランシーバは PC に送るだけの片方向通信なので TX 不要。GPIO26 は将来の双方向化に備えて予約
- `setup()` で **両方の UART を `begin()` しておく**（Serial1 は今回受信データを捨てるだけ）。これでハード改造なしにトランシーバモード実装に移行できる

GND 共通、M5Stack から STM32 / トランシーバへの電源供給はしない（別電源）。

### 内部状態 `State`

画面に出す情報とロジックが共有する構造体を 1 つに集約する。LCD はこの構造体だけ見て描画する（副作用なしの純粋関数 `lcd_draw(const State&)`）。

**将来のトランシーバ実装時にも `state.hpp` を触らなくて済むよう、GameDataRobot / GameDataInput 由来のフィールドも P1 から先に確保しておく**。

```cpp
// ---- モード ----
enum class Mode : uint8_t { GAMEPAD, TRANSCEIVER, ESTOP };

// ---- STM32 に送る指令（= AttractsCommand 相当） ----
struct Command {
  float   vx = 0, vy = 0, vz = 0;        // chassis_vel
  float   yaw = 0, pitch = 0;            // turret
  uint8_t load = 0, fire = 0;            // 0/1/2
  uint8_t speed = 0, chassis = 0;        // 0/1
};

// ---- Bluepad32 接続状況 ----
struct GamepadStatus {
  bool     connected = false;
  char     name[24]  = {};               // "Xbox Wireless" 等
  uint32_t update_ms = 0;                // 最終入力 millis()
};

// ---- トランシーバ経由のロボット情報（GameDataRobot 相当） ----
struct RobotStatus {
  bool     present = false;              // 1 回でも受信したか
  uint8_t  type    = 0;                  // 0:Tank 1:Assault 2:Marksman
  uint8_t  team    = 0;                  // 0:A 1:B
  uint8_t  projectile_speed_max = 0;     // [m/s]
  uint16_t max_hp = 0,   hp   = 0;
  uint16_t max_heat = 0, heat = 0;
};

// ---- 通信ヘルス ----
struct Comms {
  uint32_t tx_count   = 0;               // Serial2 累積送信回数
  float    tx_hz      = 0;               // 実測送信レート
  uint32_t rx1_bytes  = 0;               // Serial1 累積受信バイト数
  uint32_t rx1_frames = 0;               // デコード成功フレーム数（将来）
};

// ---- 全体 ----
struct State {
  Mode          mode = Mode::GAMEPAD;
  Command       cmd;
  GamepadStatus gp;
  RobotStatus   robot;
  Comms         comms;
};
```

### モード管理
- 起動時は **GAMEPAD モード**
- `M5.BtnA.wasPressed()` → `Mode::GAMEPAD`
- `M5.BtnC.wasPressed()` → `Mode::TRANSCEIVER`（スタブ：UART は中立値を送信）
- `M5.BtnB.wasPressed()` → `Mode::ESTOP`（全ゼロ reference を送信）
- モード切替は `State::mode` を書き換えるだけ。描画/送信ロジックは切り分けて参照する

### モジュール設計

#### 依存グラフ
```
                          +---------+
                          |  State  |  ← すべての読み書きの集約点
                          +----+----+
            書き込み元              |  読み込み元
    +------+------+---------+       +----+-------+-------+
    v      v      v         v       v            v       v
 gamepad transceiver ESTOP  mode   lcd_view   reference  (監視用 LCD)
 _mode   _mode              btn    (画面描画)   _packet
    |      |                                      |
    |      v                                      v
    |  transceiver_frame                      Serial2 → STM32
    |      |     \
    |      |      \___ crc.hpp (CRC8/CRC16) ___ reference_packet も再利用
    |      |
    v      v
 Bluepad32  Serial1 ← トランシーバ
```

- **State を唯一の真実の源**とし、入力源は State.cmd / State.robot を書き換え、出力側（LCD・UART TX）は State を読むだけ。
- **Input driver パターン**: 各モードは `XXX_mode::tick(State&, ...)` 形式に揃える（基底クラスは作らず、関数ベースで統一）。モード追加は `*_mode.hpp` を 1 本足すだけ。

#### 各ヘッダの API

**`include/state.hpp`** (P1) — 上記 `State` 系構造体のみ。依存なし

**`include/lcd_view.hpp`** (P1)
```cpp
void lcd_init();
void lcd_draw(const State& s);   // 内部 prev キャッシュで差分描画
```

**`include/crc.hpp`** (P4 で追加、将来 transceiver で再利用)
```cpp
uint8_t  crc8_xor   (const uint8_t* buf, size_t len);  // rabcl Reference
uint8_t  crc8_poly  (const uint8_t* buf, size_t len);  // transceiver ヘッダ用（多項式は実機確認で確定）
uint16_t crc16_ccitt(const uint8_t* buf, size_t len);  // transceiver ペイロード用
```
→ `reference_packet` と `transceiver_frame` の両方から参照

**`include/reference_packet.hpp`** (P4)
```cpp
// Command → 28B バイナリ（rabcl 互換、MSB-first float, XOR CRC8）
void build_reference_packet(const Command& c, uint8_t out[28]);
```

**`include/gamepad_mode.hpp`** (P5)
```cpp
namespace gamepad_mode {
  void setup();                       // Bluepad32 init + 接続/切断コールバック登録
  void tick(State& s, float dt);      // 最新コントローラ状態 → s.cmd, s.gp を更新
}
```

**`include/transceiver_frame.hpp`** (将来、P3 でスケルトンだけ)
```cpp
namespace transceiver_frame {
  // 0xAE ヘッダ + type(1) + len(2, LE) + CRC8 + payload + CRC16(LE)
  struct Input { int16_t mouse_dx=0, mouse_dy=0; uint8_t buttons=0; uint8_t keys[4]={}; };
  struct Robot { /* RobotStatus 相当 */ };
  struct Decoded { bool input_updated=false; bool robot_updated=false; Input input; Robot robot; };

  void reset();
  void feed(uint8_t byte, Decoded& out);   // Serial1.read() の 1 バイトごとに呼ぶ
}
```
- 内部ステートマシン: `WAIT_START → TYPE → LEN_LO → LEN_HI → CRC8_HDR → PAYLOAD → CRC16_LO → CRC16_HI → WAIT_START`
- P3 ではバイトを数えるだけのスケルトン（`s.comms.rx1_bytes++` 相当）
- バイト単位 feed で実装するので **ホストマシン上でユニットテスト可能**

**`include/transceiver_mode.hpp`** (将来)
```cpp
namespace transceiver_mode {
  void tick(State& s, const transceiver_frame::Decoded& d, float dt);
  // d.input を game_client 相当のロジックで s.cmd に変換
  // d.robot は s.robot に常時反映（モードに関係なく）
}
```

#### 設計のポイントと transceiver 実装時の恩恵

1. **State 分離** → 入力源を増やしても出力ロジック（LCD / UART TX）は無改修
2. **crc.hpp 独立** → P4 で XOR CRC8 を入れる際に一緒に骨組みを作っておけば、transceiver の多項式 CRC8 / CCITT-CRC16 の追加が最小差分
3. **transceiver_frame がパーサ専用** → モード切替とは無関係に Serial1 を常時パースできる（ロボット HP などは常に画面に出せる）
4. **入力ドライバの関数シグネチャを揃える** → `xxx_mode::tick(State&, ...)` という規約で main のディスパッチが `switch (s.mode)` だけで済む
5. **State 構造体に RobotStatus を P1 で先取り** → P1 で LCD に「HP --- / Heat ---」の空欄を描画しておけば、後日 transceiver 実装時に描画コードは無変更で値が埋まる

### LCD 描画内容

**解像度** 320×240 / **フォント** M5.Lcd 既定 + サイズ倍率で調整。**再描画 10 Hz**、数値欄は部分描画でちらつき回避。

#### レイアウト（縦 4 ブロック）

```
┌─────────────────────────────────────────────┐ y=0
│  MODE: GAMEPAD          GP: Xbox Wireless   │  ← ヘッダ（モード色で塗り分け）
├─────────────────────────────────────────────┤ y=30
│  Cmd                                        │
│    vx +0.00  vy +0.00  vz +0.00             │
│    yaw +0.000  pitch +0.000                 │
│    load FWD  fire LOW  spd LOW  chs NORM    │
├─────────────────────────────────────────────┤ y=130
│  Robot (transceiver)      type --- team -   │
│    HP     ----/---- ▇▇▇▇▇▇░░░░              │
│    Heat   ----/---- ▇▇░░░░░░░░              │
├─────────────────────────────────────────────┤ y=200
│  TX 100.0 Hz (#12345)   RX1 0B (0 frm)      │  ← フッター
│  [A] GP   [B] STOP   [C] TR                 │
└─────────────────────────────────────────────┘ y=240
```

Robot ブロックは P1 時点では `present=false` なので全項目 `---` 表示。transceiver 実装時にこのブロックが埋まる（描画コードは変更なし）。

#### 描画項目一覧

| ブロック | 項目 | 表示 | State フィールド |
|---|---|---|---|
| ヘッダ | モード名 | `GAMEPAD` / `TRANSCEIVER` / `ESTOP` | `mode` |
| ヘッダ | 背景色 | 緑(GP) / 黄(TR) / 赤(ESTOP) | `mode` |
| ヘッダ | GP 接続 | `GP: <name>` or `GP: ---` | `gp.connected`, `gp.name` |
| Cmd | chassis_vel | `vx/vy` [m/s], `vz` [rad/s] `+x.xx` | `cmd.vx/vy/vz` |
| Cmd | turret | `yaw/pitch` [rad] `+x.xxx` | `cmd.yaw/pitch` |
| Cmd | load | `STOP` / `FWD` / `REV` | `cmd.load` |
| Cmd | fire | `STOP` / `LOW` / `HIGH` | `cmd.fire` |
| Cmd | speed | `LOW` / `HIGH` | `cmd.speed` |
| Cmd | chassis | `NORM` / `INF` | `cmd.chassis` |
| Robot | type | `TANK` / `ASSL` / `MARK` / `---` | `robot.type`, `robot.present` |
| Robot | team | `A` / `B` / `-` | `robot.team`, `robot.present` |
| Robot | HP | `<hp>/<max>` + 横棒ゲージ | `robot.hp`, `robot.max_hp` |
| Robot | Heat | `<heat>/<max>` + 横棒ゲージ（しきい値で赤変色） | `robot.heat`, `robot.max_heat` |
| フッタ | TX レート | `TX <hz> Hz (#<count>)` | `comms.tx_hz`, `comms.tx_count` |
| フッタ | RX1 統計 | `RX1 <n>B (<frames> frm)` | `comms.rx1_bytes`, `comms.rx1_frames` |
| フッタ | ボタン凡例 | 固定文言 | — |

#### 描画関数 API（`include/lcd_view.hpp`）
- `void lcd_init();` — 起動時 1 回、枠・ラベル等の固定要素を描画
- `void lcd_draw(const State& s);` — 値部分だけ更新。前回値をキャッシュして差分描画でちらつき抑制

### 開発スタイル

**TDD（テスト駆動開発）を可能な範囲で適用**する。

- 新しい純粋ロジックモジュールは **テストを先に書いて失敗させ、その後実装して green にする**
- ハード依存（`M5.Lcd`, `Serial`, `M5.Btn`, Bluepad32 など）はテストが非現実的なので **実機確認のみ**
- テスト可能にするため、LCD の中でも純粋なラベル変換などは `labels.hpp` 等の独立ヘッダに切り出す
- 毎コミット前に `pio run`（m5stack 用）と `pio test -e native`（テスト追加後）の両方が通ることを確認

テスト対応表:

| 種別 | 例 | テスト方式 |
|---|---|---|
| 純粋ロジック | `crc.hpp`, `reference_packet.hpp`, `labels.hpp`, `transceiver_frame.hpp`, gamepad マッピング | ホスト native テスト（Unity + ArduinoFake） |
| ハード依存 | `lcd_view.hpp`, UART I/O, Bluepad32, M5.Btn | 実機目視 + ロジアナ |

### 実装フェーズ（順序）

**凡例**: ◎ = TDD（test-first） / □ = ハード実機確認 / 📝 = プラン/設定のみ

| # | やること | 種別 | 成果物 | 確認方法 |
|---|---|---|---|---|
| **0** | プロジェクト初期化 | 📝 | `.gitignore`, `docs/plan.md` | `git status` クリーン |
| 1a | プロジェクトひな形 | □ | `platformio.ini` (m5stack env), 最小 `src/main.cpp` | `pio run`、M5Stack で真っ黒画面 |
| 1b | **native テスト環境構築** | 📝 | `platformio.ini` に `[env:native]` (Unity + ArduinoFake)、空の `test/test_native/test_main.cpp` | `pio test -e native` が 1 件 dummy で通る |
| 1c | **TDD: labels.hpp** | ◎ | test 先行 → `include/labels.hpp`（`mode_label(Mode)`, `load_label(uint8_t)`, `fire_label(uint8_t)`, `speed_label(uint8_t)`, `chassis_label(uint8_t)`, `type_label(RobotStatus)`, `team_label(RobotStatus)`） | ラベル変換テストが green |
| 1d | State 構造体追加 | 📝 | `include/state.hpp`（RobotStatus 含む完全版）、`main.cpp` で宣言 | `pio run` 成功 |
| 1e | LCD 静的レイアウト | □ | `include/lcd_view.hpp`（labels を利用）、`main.cpp` で `lcd_init()/lcd_draw(s)` | 画面にヘッダ/Cmd/Robot/フッタのラベル、Robot 欄は `---` |
| 1f | ダミー値アニメ | □ | `main.cpp` に `s.cmd.vx = sinf(t)` 等 | 数値が滑らかに動く、ちらつきなし |
| 2  | モード切替ボタン | □ | BtnA/B/C → `s.mode` 更新 | ヘッダ色・文字が切り替わる |
| 3  | Serial1/2 init + Serial1 受信スケルトン | □ | setup 内で両 UART init、`s.comms.rx1_bytes += n` | USB-UART で GPIO36 にデータ入力 → LCD の RX1 増加 |
| 4a | **TDD: crc.hpp** | ◎ | test 先行 → `include/crc.hpp`（`crc8_xor`, `crc8_poly`, `crc16_ccitt`） | 既知ベクタテストが green |
| 4b | **TDD: reference_packet.hpp** | ◎ | test 先行 → `include/reference_packet.hpp` | `Command` 値 → 期待 28 byte 列のテストが green（rabcl 実装と突合） |
| 4c | 100Hz 送信統合 | □ | main.cpp の loop で 10ms 毎に `Serial2.write`、`tx_hz` 計測 | ロジアナで `A5 5A ... CRC 00` が 10ms 周期、LCD の TX Hz 更新 |
| 5a | **TDD: gamepad マッピング** | ◎ | test 先行 → `include/gamepad_mode.hpp` の純粋 `update_from_axes(...)` 部分 | 軸/ボタン入力 → `Command` 値のテストが green |
| 5b | Bluepad32 接続統合 | □ | `platformio.ini` に Bluepad32 追加、Connect/Disconnect コールバック、上記純粋関数を呼ぶ | コントローラ操作で LCD の `vx/yaw/...` 追随、beta_main 実機で動く |
| 将来 | transceiver_frame / transceiver_mode | ◎ + □ | パーサは test-first、受信処理統合は実機 | パーサテスト green、実機で GameDataRobot 表示 |

**各フェーズで独立にコミット可能**。毎コミット前に `pio run` と（追加後は）`pio test -e native` の両方を通す。P1e 完了時点で「内部状態を可視化するダッシュボード」の土台が動く。

#### P0: `.gitignore` 内容（PlatformIO 既定＋ちょい足し）
```gitignore
# PlatformIO
.pio/
.pioenvs/
.piolibdeps/

# VSCode 自動生成
.vscode/.browse.c_cpp.db*
.vscode/c_cpp_properties.json
.vscode/launch.json
.vscode/ipch

# ビルド成果物
*.o
*.bin
*.elf
```

#### P0: プラン書類の保存先
- `m5_basic_2_7_attracts/docs/plan.md` として本プラン（`C:/Users/kota0/.claude/plans/recursive-yawning-moore.md`）をコピーしてコミット
- 以降の更新は docs/plan.md 側で行う（Claude の plans フォルダは Plan モード作業用のワーキングコピー扱い）

### Reference パケット（M5Stack → STM32）
`beta_main/lib/rabcl/src/interface/uart.cpp` の `PrepareReferencePacket` と完全互換で実装する。28 byte:

| off | size | 内容 | 備考 |
|---|---|---|---|
| 0 | 1 | 0xA5 | HEADER_0 |
| 1 | 1 | 0x5A | HEADER_1 |
| 2 | 4 | chassis_vel_x | float big-endian |
| 6 | 4 | chassis_vel_y | float big-endian |
| 10 | 4 | chassis_vel_z | float big-endian |
| 14 | 4 | yaw_pos | float big-endian |
| 18 | 4 | pitch_pos | float big-endian |
| 22 | 1 | load_mode | 0/1/2 |
| 23 | 1 | fire_mode | 0/1/2 |
| 24 | 1 | speed_mode | 0/1 |
| 25 | 1 | chassis_mode | 0/1 |
| 26 | 1 | CRC8 | XOR of [2..25] |
| 27 | 1 | 0x00 | padding |

float エンコードは IEEE754 を `uint32_t` に再解釈して **MSB first** で書き込む（rabcl `EncodeFloat` と同じ）。

### 送信周期
- **100 Hz**（10 ms）— beta_main の制御周期に合わせる
- Bluepad32 のコールバックで受けたゲームパッド状態を main loop で最新値として使う（割込排他は Bluepad32 内部でケア済）

### ゲームパッド → AttractsCommand マッピング
`attracts_interface/src/gamepad_node.cpp` の仕様を踏襲:

```
chassis_vel_x = axes[1] * MAX_OMNI_VEL           // 左スティック縦
chassis_vel_y = axes[0] * MAX_OMNI_VEL           // 左スティック横
// D-pad が非ゼロなら上書き
if (axes[7] != 0) chassis_vel_x = axes[7] * MAX_OMNI_VEL
if (axes[6] != 0) chassis_vel_y = axes[6] * MAX_OMNI_VEL

chassis_vel_z = 0.5 * (axes[5] - axes[2]) * MAX_OMNI_ROT_VEL   // トリガー

yaw_pos   = wrap_2pi(yaw_pos   + axes[3] / 10.0 * DT_GAIN)     // 右スティック横 → 積算
pitch_pos = clamp(pitch_pos + axes[4] / 20.0 * DT_GAIN, -π/12, π/6)

fire_mode    = buttons[5] ? 1 : 0
load_mode    = buttons[0] ? 2 : (buttons[4] ? 1 : 0)
chassis_mode = buttons[1] ? 1 : 0
speed_mode   = 0   // gamepad_node に合わせ固定
```

Bluepad32 は統一 API（`gp->axisX()`, `gp->dpad()`, `gp->buttons()` 等）を返すので、`sensor_msgs/Joy` の axes/buttons インデックスをそのマッピングに置き換える。具体的な対応（Xbox 配列ベース）:

| Joy index | 内容 | Bluepad32 |
|---|---|---|
| axes[0] | 左X | `axisX()` (-512..511 を -1..1 に正規化) |
| axes[1] | 左Y | `axisY()` (符号反転、上が +) |
| axes[2] | LT | `brake()` (0..1023 を 0..-1 または 1..-1 に) |
| axes[3] | 右X | `axisRX()` |
| axes[4] | 右Y | `axisRY()` (符号反転) |
| axes[5] | RT | `throttle()` |
| axes[6] | D-pad X | `dpad()` bit (LEFT=-1, RIGHT=+1) |
| axes[7] | D-pad Y | `dpad()` bit (UP=+1, DOWN=-1) |
| buttons[0] | A | BUTTON_A |
| buttons[1] | B | BUTTON_B |
| buttons[4] | LB | BUTTON_SHOULDER_L |
| buttons[5] | RB | BUTTON_SHOULDER_R |

定数（後でチューニング）:
- `MAX_OMNI_VEL = 1.0f` [m/s]
- `MAX_OMNI_ROT_VEL = 1.0f` [rad/s]
- `DT_GAIN = 1.0f`（毎 10 ms 呼ばれるので軸/10 → 0.01 rad/tick ≒ 1 rad/s max yaw rate）

---

## 作成ファイル

新規作成のみ（編集対象の既存ファイルはなし）。フェーズ単位で順に追加していく:

```
m5_basic_2_7_attracts/
├── .gitignore                  # .pio/, .vscode/ 自動生成系を除外               [P0]
├── docs/
│   └── plan.md                 # 本プラン書類をリポジトリに保存                 [P0]
├── platformio.ini              # [env:m5stack-core-esp32] + [env:native]        [P1a/P1b/P5b]
├── src/
│   └── main.cpp                # setup / loop / モード管理 / 全体統合            [P1a→徐々に肥える]
├── include/
│   ├── state.hpp               # State, Command, GamepadStatus, RobotStatus    [P1d]
│   ├── labels.hpp              # ラベル変換（TDD）                              [P1c]
│   ├── lcd_view.hpp            # lcd_init / lcd_draw(const State&)             [P1e]
│   ├── crc.hpp                 # CRC8 XOR / CRC8 poly / CRC16 CCITT (TDD)      [P4a]
│   ├── reference_packet.hpp    # rabcl Reference 28B エンコード (TDD)          [P4b]
│   ├── gamepad_mode.hpp        # Bluepad32 + 純粋マッピング (TDD)              [P5]
│   ├── transceiver_frame.hpp   # 0xAE フレームパーサ (TDD)                     [将来]
│   └── transceiver_mode.hpp    # Decoded → State                                [将来]
└── test/
    └── test_native/
        ├── test_main.cpp                   # Unity エントリ                     [P1b]
        ├── test_labels.cpp                 # labels.hpp テスト                  [P1c]
        ├── test_crc.cpp                    # crc.hpp テスト                     [P4a]
        ├── test_reference_packet.cpp       # reference_packet.hpp テスト        [P4b]
        └── test_gamepad_mapping.cpp        # gamepad 純粋マッピングテスト       [P5a]
```

末尾 `[Px]` は上記の実装フェーズ番号。各 TDD フェーズでは `test_*.cpp` を先に書き、green にしてから実装が完成する。

### `platformio.ini`
```ini
[env:m5stack-core-esp32]
platform = espressif32
board = m5stack-core-esp32
framework = arduino
monitor_speed = 115200
upload_speed = 921600
lib_deps =
    m5stack/M5Stack
    ; bluepad32/Bluepad32 ; P5b で追加
build_flags =
    -DCORE_DEBUG_LEVEL=0

[env:native]
platform = native
test_framework = unity
lib_deps =
    fabiobatsilva/ArduinoFake
build_flags =
    -std=gnu++17
```

### `include/state.hpp`（P1）
- 上記「内部状態 `State`」節の `struct State` 定義のみ。依存なし・純粋なデータ

### `include/lcd_view.hpp`（P1）
- `lcd_init()` / `lcd_draw(const State&)` のみを提供
- 前回値キャッシュで差分描画
- M5Stack ライブラリのみに依存

### `include/reference_packet.hpp`（P4）
- `void build_reference_packet(const State& s, uint8_t out[28]);`
- `EncodeFloat`（MSB first）と `CalcCrc8`（XOR）は rabcl と同等実装を複製（サブモジュール取込は ESP32 ビルドの複雑化を招くため単一ヘッダに最小コピー）
- 参照元: `beta_main/lib/rabcl/src/interface/uart.cpp:33-44` (EncodeFloat), `:24-31` (CalcCrc8), `:90-108` (PrepareReferencePacket)

### `include/gamepad_mode.hpp`（P5）
- `void update_state_from_gamepad(ControllerPtr gp, State& s, float dt);`
- yaw/pitch は `State` をステートとして積算
- 参照元: `attracts_interface/src/gamepad_node.cpp` の `UpdateCmdVel`

### `src/main.cpp` 最終形（P5 終了時）
- `setup()`:
  - `M5.begin()` → `lcd_init()`
  - `Serial2.begin(230400, SERIAL_8N1, 16, 17)` — STM32 用
  - `Serial1.begin(115200, SERIAL_8N1, 36, 26)` — トランシーバ用（ピン確保、受信読み捨て）
  - `BP32.setup(onConnect, onDisconnect)`
- `loop()`: 10 ms 周期で
  1. `M5.update()` → ボタンで `state.mode` 更新
  2. モード別に state のコマンド欄を更新
    - GAMEPAD: `update_state_from_gamepad`
    - TRANSCEIVER: 中立値
    - ESTOP: 全ゼロ
  3. `build_reference_packet(state.cmd, buf)` → `Serial2.write(buf, 28)`、`state.comms.tx_count++` と `tx_hz` 更新
  4. `Serial1.available()` → `transceiver_frame::feed(...)` でバイト投入、`state.comms.rx1_bytes += n`
  5. 10 Hz で `lcd_draw(state)`

---

## 動作確認（各フェーズ終了時）

**P1 — State + LCD**
- `pio run --target upload` 後、LCD にヘッダ/ボディ/フッタのレイアウトが出ていること
- `loop()` 内のダミー値（`s.cmd.vx = sinf(millis()/1000.0f)` 等）で数値が滑らかに動くこと
- ちらつきがないこと（差分描画が効いている）

**P2 — モード切替**
- BtnA で GAMEPAD（緑）、BtnC で TRANSCEIVER（黄）、BtnB で ESTOP（赤）にヘッダが切り替わる

**P3 — UART 初期化**
- `pio device monitor` で `Serial2 begin OK, Serial1 begin OK` 的なログ
- ロジアナで GPIO17 に 230400 bps の波形が出ていること（中身は仮でよい）

**P4 — Reference パケット 100Hz**
- ロジアナで `A5 5A .. CRC 00` の 28 byte が 10 ms 周期で流れる
- beta_main を繋いだ状態で、STM32 側の IMU フィードバック等が動く（UART 応答を別途モニタ）
- LCD フッタの `TX 100.0 Hz` と `#count` の増加を確認

**P5 — ゲームパッド接続**
- Bluepad32 でコントローラをペアリングすると LCD ヘッダに `GP: <name>` が出る
- スティックを倒すと LCD の `vx/vy/vz/yaw/pitch` が追随
- beta_main 実機で想定どおり動く（chassis 旋回、turret 追従、load/fire モード切替）

---

## スコープ外（今回やらないこと）

- transceiver モードの実装（0xAE フレーム受信・GameDataRobot → LCD 表示）— **ピンと `Serial1.begin()` だけ先に入れておく**
- WiFi / micro-ROS 連携
- ゲームパッド機種別のボタン/軸マッピング差異吸収（一旦 Xbox レイアウト決め打ち）
- `Feedback` パケット（STM32 → M5Stack）受信と LCD 表示
- 振動フィードバック、スピーカー通知

これらは次フェーズで。
