# M5-ATEM-TarryLight
ATEM Mini Tally Light Using M5StickC
M5StickC を使った ATEM Mini 用タリーランプです。

ATEM スイッチャーへ Wi-Fi 経由で接続し、指定したカメラ番号のプログラム／プレビュー状態を
M5StickC の LCD 全面の色と本体 LED で表示します。

## 表示仕様

| 状態 | 画面 | 本体 LED |
| --- | --- | --- |
| プログラム（オンエア中） | 赤背景に黒のカメラ番号 | 点灯 |
| プレビューのみ | 緑背景に黒のカメラ番号 | 消灯 |
| どちらでもない | 白背景にグレーのカメラ番号 | 消灯 |

## 必要なもの

- M5StickC（本体のみ）
- ATEM Mini / ATEM Mini Pro などの ATEM スイッチャー
- スイッチャーと同じネットワークに接続できる Wi-Fi アクセスポイント（2.4GHz）

## 開発環境・ライブラリ

Arduino IDE でビルドします。以下をインストールしてください。

1. **ESP32 ボードマネージャ**（M5StickC 用のボード定義と `WiFi.h` を含みます）
2. **M5StickC** ライブラリ（Arduino IDE のライブラリマネージャからインストール）
3. **SKAARHOJ ATEM ライブラリ** — `ATEMbase`、`ATEMstd`、`SkaarhojPgmspace`
   （[SKAARHOJ-Open-Engineering](https://github.com/kasperskaarhoj/SKAARHOJ-Open-Engineering) の
   `ArduinoLibs` 以下にあるライブラリを Arduino の `libraries` フォルダへコピーします）

ボード設定は「M5Stick-C」を選択してください。

## セットアップ

`M5-ATEM-TarryLight.ino` の以下の箇所を、利用する環境に合わせて書き換えます。

```cpp
IPAddress switcherIp(192, 168, 24, 210);   // ATEM スイッチャーの IP アドレス

const char* ssid = "APSSID";               // Wi-Fi の SSID
const char* password =  "**password**";    // Wi-Fi のパスワード

int cameraNumber = 1;                      // 起動時に監視するカメラ番号
```

書き換えたら M5StickC へ書き込みます。起動時に Wi-Fi へ接続し、接続に成功すると
IP アドレスが表示されたあとタリー表示に切り替わります。接続できない場合は
`Connection Failed.` と MAC アドレスが表示されます（接続試行は約 15 秒でタイムアウトします）。

## 使い方

- **ボタン A（M5 ボタン）**: カメラ番号を切り替えます（1 → 2 → 3 → 4 → 1）
- **ボタン B（横のボタン）**: 情報表示モードとタリー表示モードを切り替えます
  - 情報表示モードでは、IP アドレス・MAC アドレス・バッテリー残量を表示します
- **ボタン B を 2 秒以上長押し**: 画面に `ReConncet` を表示します

Copyright (c) Jun SUZUKI

