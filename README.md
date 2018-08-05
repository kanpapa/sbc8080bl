# sbc8080bl
SBC8080BL

## 概要
電脳伝説さんが開発したSBC8080用Boot Loader REV02を実験しています。

## 内容
* AdruinoスケッチをAE-ATMEGA328-MINIに書き込んで、SBC8080BLボードに実装して動かします。
* サンプルスケッチに組み込まれているプログラムをHEXフォーマットにしてSDカードに書いています。

## 状況
* SDカードから読み込めるようにスケッチを書き換えてみたのですが、まだうまく動きません。
* SDカードのアクセス部分をコメントにすると問題なく動きます。
* うまく動かないときも153.9KHzのRXD/TXDは正常に生成されています。