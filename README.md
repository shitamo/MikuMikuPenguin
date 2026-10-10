[![Build Status](https://travis-ci.org/sn0w75/MikuMikuPenguin.png)](https://travis-ci.org/sn0w75/MikuMikuPenguin)

**WARNING**
THIS FILE IS ENCODED IN UTF-8, NOT SJIS!!!

**注意**
このファイルはSJISではなくUTF-8でエンコードされています！！！

English translation is [available](http://github.com/sn0w75/mikumikupenguin/blob/master/README.en.md).

# LibMikuMikuPenguin(libmmp) v0.2 リードミ（日本語）

どうも初めまして。sn0w75です。

自分の好きなOSでミクミクしたくて、それを出来る為のライブラリやプログラムを作ってみました。
まだバグだらけですし、APIが確定しているわけでもないのでMMPをライブラリとして使うのはまだまだ早いかと思います。
ですが、これを起点として、Windows以外のOSでのMMDの開発が段々進めていったら～と思ってます。

最新のコードは以下のリンクで手に入れます：
https://github.com/sn0w75/MikuMikuPenguin/

## 2026年 現行環境対応版について

このツリーは v0.2.1 をベースに、現在の Linux で動くよう次の変更を加えたものです。

* GLFW 2 → GLFW 3 へ移行、新しい GLM (0.9.9 / 1.x) に対応
* ボーン行列をテクスチャバッファで渡すように変更（ボーン数 251 本の上限を撤廃）
* 頂点数 32767 超のモデルでのクラッシュ、テクスチャ番号 -1 などのクラッシュを修正
* テクスチャ名の大文字小文字を区別せずに検索
* 再生速度をモニタのリフレッシュレートに依存しないよう修正
* autotools は pkg-config で依存を検出（git から取得した場合は `./autogen.sh` を実行）
* Debian パッケージ（debian/）を追加

必要なもの: GLFW 3, GLEW, GLM, Bullet, stb_image（音楽再生には SDL2, SDL2_mixer）、OpenGL 3.2 core profile

## ディレクトリ構成

中身はこんな感じです：

* COPYING: このコードのライセンス
* doc: ドキュメンテーションを入れる為のフォルダですが、現時点では得に何も入ってません。
* include: LibMikuMikuPenguinのヘッダ（.h）ファイルがここに入ってます。
* INSTALL: このコードのインストール仕方（英語のみ）
* libmmp: LibMikuMikuPenguinのソース（.cpp）ファイルがここに入ってます。
* shaders: PMXViewerのシェーダーがここに入ってます。
* src: PMXViewerのソースコード（.cpp、.h）がここに入ってます。
* data: 必要なテクスチャ等が入ってます。

## 依存ライブラリ

LibMikuMikuPenguinにはlibglfw, libglew,libglm,libBulletが必要です。

PMXViewerにはlibmmp、stb_image（libstb）が必要です（2026年版で SOIL から変更）。VMDと一緒に音楽を再生したい場合は、SDL2、SDL2_mixerも必要です。（なくてもコンパイル出来ます）

### ライブラリの詳細（2014年1月20日）：

* libglfw: 現在では2.7が必要です。3.0+は2.xと非対応です。
* libglm: 現在では0.9.4(0.9.2、 0.9.3でも大丈夫かもしれません）が必要です。0.9.5+は0.9.4と非対応です。

**注意！！！！！**

GLM 0.9.3（<=0.9.2もそうかもしれない）ではバグがあります： 
https://github.com/g-truc/glm/issues/15

このバグを回避する為には、-fpermissiveを使う必要があります。例えば：

    $ export CXXFLAGS="-fpermissive"
    $ autoreconf -i
    $ ./configure
    # make install

みたいな感じです。

## インストール
このソースコードはautotoolsを利用していますので、コンパイルやインストールは簡単です：

コンパイルするにはターミナルで：

    $ autoreconf -i
    $ ./configure
    $ make

をすれば大丈夫です。
またシステムにインストールしたい場合は：

    $ sudo make install

みたいな感じで大丈夫です。

***注意！！！***
現在WindowsでビルドするにはMinGWを使う必要があります。詳細についてはsn0w75氏を連絡。
（TODO: Windowsでの説示を書く）

## あなたはどのブランチを使うべきか?

今のところ、 あなたのコンピュータが、どのバージョンのOpenGLを使える
かによって、コンパイルするべき git branch は異なります。

### 確認方法

#### Linux

Linux での確認方法は、コマンドラインで、

     glxinfo | grep OpenGL

を実行して、

     OpenGL vendor string: Tungsten Graphics, Inc
     OpenGL renderer string: Mesa DRI Intel(R) Ironlake Mobile x86/MMX/SSE2
     OpenGL version string: 2.1 Mesa 8.0.5
     OpenGL shading language version string: 1.20
     OpenGL extensions:

とでてきたら、version strings に注目します。

例示はあなたのコンピュータの OpenGL は Version 2.1
だと示しています。

### ブランチの切り替え方法

git を使ったことがないなら、<http://progit-ja.github.io/> などで
学ぶのがいいでしょう。

git clone した直後には

    git checkout -b legacy-210 origin/legacy-210

を実行し、ローカルにブランチを作ります。確認方法は

    git branch -a

です。2回目以降は、git checkout を上記のサイトや書籍などで
学んでください。
遅かれ早かれ、ブランチの更新も勉強することになるでしょうから。

## 実行出来ない、パフォーマンスが悪かった場合：
* OSXでは、頂点シェ－ダーでボーン行列の数が多すぎるとソフト側で頂点シェーディングを行ってしまう場合があります。こういう場合、[shaders/model.vert](https://github.com/sn0w75/MikuMikuPenguin/blob/master/shaders/model.vert#L17)でBones（行列の配列）の行列の数を減らすことで回避出来ますが、少なすぎるとボーン変形でモデルが破損する場合があります。ご注意ください。
* legacy-210ブランチを使えばOpenGL 2.1しか対応していない環境でもPMXViewerを使えるようになりますが、現代だとそんなような環境はバーチャル環境ぐらいです。OpenGL 2.1やバーチャル環境でのパフォーマンスについては何も保証出来ませんのでご了承ください。


他に質問がありました、気軽にメッセージ飛ばしてください。喜んで手伝います：
* Twitter [@sn0w75](http://twitter.com/sn0w75)
* E-mail <ibenrunnin@gmail.com>

