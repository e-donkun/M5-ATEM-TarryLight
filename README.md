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
| Wi-Fi 未接続 | 黒背景に `No WiFi` と MAC アドレス | 消灯 |
| スイッチャー未接続 | 黒背景に `No ATEM` と接続先 IP アドレス | 消灯 |

プログラムとプレビューが同じ入力になっている場合は、プログラム（赤）を優先して表示します。
接続が切れている間は白（非選択）ではなく専用の画面を表示するため、
「選ばれていない」のか「タリーが届いていない」のかを取り違えることはありません。

## 必要なもの

- M5StickC（本体のみ）
- ATEM Mini / ATEM Mini Pro などの ATEM スイッチャー
- スイッチャーと同じネットワークに接続できる Wi-Fi アクセスポイント（2.4GHz）

## 開発環境・ライブラリ

Arduino IDE または Arduino Cloud でビルドします。以下をインストールしてください。

1. **ESP32 ボードマネージャ**（M5StickC 用のボード定義と `WiFi.h` を含みます）
2. **M5StickC** ライブラリ（ライブラリマネージャからインストール）
3. **SKAARHOJ ATEM ライブラリ** — `ATEMbase`、`ATEMstd`、`SkaarhojPgmspace`
   （[SKAARHOJ-Open-Engineering](https://github.com/kasperskaarhoj/SKAARHOJ-Open-Engineering) の
   `ArduinoLibs` 以下にあるライブラリを Arduino の `libraries` フォルダへコピー、
   または Arduino Cloud にカスタムライブラリとしてアップロードします）

ボード設定は「M5Stick-C」を選択してください。

## セットアップ

Wi-Fi の SSID とパスワードは `arduino_secrets.h` に書きます。
Arduino Cloud では、この 2 つはスケッチの Secret タブで編集され、スケッチ本体には保存されません。

```cpp
#define SECRET_SSID "APSSID"
#define SECRET_PASS "**password**"
```

スイッチャーの IP アドレスやカメラ台数などは
`M5-ATEM-TarryLight.ino` 冒頭の定数を書き換えます。

```cpp
static const IPAddress kSwitcherIp(192, 168, 24, 210);  // ATEM スイッチャーの IP アドレス
static const uint8_t kCameraCount = 4;                  // 切り替えるカメラ入力の数
```

書き換えたら M5StickC へ書き込みます。起動時に Wi-Fi へ接続し（タイムアウトは約 15 秒）、
接続できない場合も動作は継続し、10 秒ごとに自動で再試行します。
スイッチャーとの接続が切れた場合もライブラリが自動で再接続します。

## 使い方

- **ボタン A（M5 ボタン）**: カメラ番号を切り替えます（1 → 2 → 3 → 4 → 1）
- **ボタン B（横のボタン）を短く押す**: 情報表示モードとタリー表示モードを切り替えます
  - 情報表示モードでは、カメラ番号・スイッチャーとの接続状態・IP アドレス・
    MAC アドレス・バッテリー残量を表示します
- **ボタン B を 2 秒以上長押し**: Wi-Fi とスイッチャーへ接続し直します

## トラブルシューティング

### `fatal error: WifiUDP.h: No such file or directory`

`ATEMbase.h` が `<WifiUDP.h>` を include していますが、ESP32／ESP8266 のコアが提供する
ヘッダ名は `WiFiUdp.h` です。Windows や macOS はファイルシステムが大文字小文字を区別しない
ため気づかれませんが、Linux 上でビルドする Arduino Cloud では include に失敗します。

ライブラリの `ATEMbase.h` を修正してアップロードし直してください。

```diff
-#include <WifiUDP.h>
+#include <WiFiUdp.h>
```

この include は `#ifdef ESP8266` の中にあり、M5StickC（ESP32）向けのビルドでは成立しません。
`ATEMbase.h` と `ATEMbase.cpp` の両方で条件を広げないと、今度は Ethernet 側が選ばれて
`EthernetUdp.h` が見つからないというエラーになります。

```diff
-#ifdef ESP8266
+#if defined(ESP8266) || defined(ESP32)
```

## License

This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version. See [LICENSE](LICENSE) for details.

Copyright (c) 2020 Jun SUZUKI
