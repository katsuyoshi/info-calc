# 情報電卓 (Info Calc)

## 概要

電卓を様々な情報を表示する表示器にします。  
ESP NOWで外部からデータを受信することで温度や湿度など様々なデータを表示できます。  
[タイマー表示させるサンプル](platformio/timer_publisher)をM5StickCに書き込むみAボタンをおしてタイマーをスタートさせるとタイマー表示が確認できます。

protopediaに登録していますので、そちらもご覧ください。

[情報電卓](https://protopedia.net/prototype/4227)


## 材料

- 電卓 Canon WS-1200H (恐らく廃品)
- M5Atom Matrix x 1
- FS90 x 3

![](/images/7D102AE0.png)

## 作り方

### 部品印刷


以下の2つの3Dモデルデータを3Dプリンターで印刷します。  
電卓によって変わるので設計し直しが必要と思われます。(データは参考程度です。)  

- [base_and_pushers.gcode](/3dmodels/base_and_pushers.gcode)
- [spacer.gcode](/3dmodels/spacer.gcode)

M2x6mmのネジでサイドの押さえと上面を止めます。  
サーボモーターもM2x6mmで止めます。  

棒状のサーボホーンとプッシャーをサーボモーター付属のネジで止めます。  

### 配線

以下の組みあせで配線します。

|M5Atom Matrix|FS90(=,+用)|FS90(.,0用)|FS90(1,CA用)|FS90(,-用)|備考|
|:--|:--|:--|:--|:--|:--|
|G22|橙||||100Ωの抵抗を間に入れてます|
|G19||橙|||100Ωの抵抗を間に入れてます|
|G23|||橙||100Ωの抵抗を間に入れてます|
|G33||||橙|100Ωの抵抗を間に入れてます|
|5V|赤|赤|赤|赤||
|GND|茶|茶|茶|茶||

### ソフトウェア書き込み

- このリポジトリをダウンロードします。
- VSCodeにPlatformIO拡張モジュールインストールし、[platformio/info_calc](/platformio/info_calc)フォルダを開きます。  
- 必要なライブラリーなど自動で読み込まれますので終わるまで待ちます。(下部ステータスバーでローディングのアニメーションが見えてる間)
- env.h.sampleをコピーしenv.hを作成します。
- 定数定義にSSIDとパスワードを書き込みます。
- USBケーブルでPCとM5Atom Matrixを繋ぎます。
- 下部ステータスバーの書き込みアイコン(レ点)を押して書き込みます。


