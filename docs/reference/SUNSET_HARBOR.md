# 夕景デモと3Dアセット比較

## 遊ぶ

`SunsetWalk.exe` は自作エンジンの Windows / Direct3D 12 で動く夕景の港です。標準Windows Actionsの成果物に含まれる `shaders` を実行ファイルの隣に置きます。WASDで移動、Tabでマウス視点、F2で簡素・詳細モデルを切り替え、Rで開始位置、Escで終了します。

自分でビルドする場合はMSVC 2022、CMakeとDXCが必要です。ルートで実行します。

```powershell
cmake -S . -B build-sunset -G "Visual Studio 17 2022" -A x64 -DGAME_PROFILE=all -DBUILD_SUNSET_WALK=ON
cmake --build build-sunset --config Release --target sunset_walk
./build-sunset/Release/SunsetWalk.exe
```

## 判断と検証

- 夕景を先に見せるため、同一の歩行ルート上に低い太陽、海面の光沢、繰り返す石のアーチを配置した。既存のPBRとシャドウマップを利用し、海面の明るい部分は法線マップと直接光による鏡面ハイライトで作る。
- 高品質なPVへの寄与を調べるため、アーチと歩道の寸法・個数・衝突領域、照明・海・背景、レンダラー、解像度とカメラを固定した。簡素版は大きな単色面と8分割のアーチ、詳細版は石積み・床の継ぎ目・24分割のアーチと128平方画素の手続きテクスチャを使う。素材は同じ工程の比較用にソースから生成する。
- 広角と近景の両方を検査する。検査は画素差、建築部分の三角形数、WARPで記録・送信・完了待ちを含むフレーム時間のサンプルを出す。画素差は表現の違いを示す。見た目の評価では同一表示サイズのA/B画像を見比べ、読み取れる材質、形のシルエット、継ぎ目を観察する。WARP時間はGPU実機の測定値に流用しない。
- 条件は地形・照明・素材を限定した単一の場面と2つの静止画。詳細版はジオメトリと材質の両方を変えるため、各要素の寄与には別々の比較が必要。PVの音楽・動画編集・視聴者評価と、DXRを使う描画は今後の確認。

```powershell
./build-sunset/Release/SunsetWalk.exe --self-test
./build-sunset/Release/SunsetWalk.exe --capture sunset-evidence
python -B tools/sunset/verify_captures.py sunset-evidence
```

`--self-test` は移動、柱の衝突、歩行範囲を検査します。`--capture` は新しいディレクトリに同じ描画パスのWARP出力を保存します。CIはPNG・CSV・JSONと実行用ファイルを短期保持します。

## AIの利用

Codexが試作コード、比較スクリプトとCIの下書きを作成した。実験の課題設定は、ユーザーが指定した光表現、PV、公開環境での防御、アセット品質の比較に基づく。観察と数値はビルドとキャプチャを実行した結果にひも付ける。
