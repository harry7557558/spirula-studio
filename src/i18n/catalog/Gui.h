#pragma once

// The application's own copy: menu bar, home screen, trainer, viewport,
// status strip, modals.
//
// Entries still written as SS_MSG_EN are English-only and are the remaining
// translation work; `bash tools/check_i18n.sh` counts them. Everything else
// carries all thirteen languages and cannot compile without them.
//
// Two rules that are expensive to retrofit, so they are followed from the
// start (see src/i18n/Message.h):
//   * never concatenate sentence fragments -- one message per sentence, with
//     {0} / {1} placeholders, because every language here reorders clauses and
//     three of them are verb-final;
//   * no plural-sensitive sentences -- "Objects: 3", not "3 objects", or
//     Russian needs a three-form plural rule and every counting message
//     triples.

#include "i18n/BeginCatalog.h"

namespace spirula {
namespace i18n {
namespace msg {
namespace gui {

// ===========================================================================
// Menu bar
// ===========================================================================

SS_MSG(menu_file,
    EN("File"),          JA("ファイル"),      ZH_HANS("文件"),     ZH_HANT("檔案"),
    KO("파일"),           DE("Datei"),        FR("Fichier"),      ES("Archivo"),
    PT("Arquivo"),       IT("File"),         NL("Bestand"),      RU("Файл"),
    TR("Dosya"));

SS_MSG(menu_view,
    EN("View"),          JA("表示"),          ZH_HANS("视图"),     ZH_HANT("檢視"),
    KO("보기"),           DE("Ansicht"),      FR("Affichage"),    ES("Ver"),
    PT("Exibir"),        IT("Visualizza"),   NL("Beeld"),        RU("Вид"),
    TR("Görünüm"));

SS_MSG(menu_help,
    EN("Help"),          JA("ヘルプ"),        ZH_HANS("帮助"),     ZH_HANT("說明"),
    KO("도움말"),         DE("Hilfe"),        FR("Aide"),         ES("Ayuda"),
    PT("Ajuda"),         IT("Aiuto"),        NL("Help"),         RU("Справка"),
    TR("Yardım"));

SS_MSG(menu_language,
    EN("Language"),      JA("言語"),          ZH_HANS("语言"),     ZH_HANT("語言"),
    KO("언어"),           DE("Sprache"),      FR("Langue"),       ES("Idioma"),
    PT("Idioma"),        IT("Lingua"),       NL("Taal"),         RU("Язык"),
    TR("Dil"));

SS_MSG(menu_about,
    EN("About"),         JA("このアプリについて"), ZH_HANS("关于"),  ZH_HANT("關於"),
    KO("정보"),           DE("Über"),         FR("À propos"),     ES("Acerca de"),
    PT("Sobre"),         IT("Informazioni"), NL("Over"),         RU("О программе"),
    TR("Hakkında"));

SS_MSG(about_version,
    EN("Version: {0}"), JA("バージョン: {0}"), ZH_HANS("版本：{0}"), ZH_HANT("版本：{0}"),
    KO("버전: {0}"), DE("Version: {0}"), FR("Version : {0}"), ES("Versión: {0}"),
    PT("Versão: {0}"), IT("Versione: {0}"), NL("Versie: {0}"), RU("Версия: {0}"),
    TR("Sürüm: {0}"));

SS_MSG(menu_quit,
    EN("Quit"),          JA("終了"),          ZH_HANS("退出"),     ZH_HANT("結束"),
    KO("종료"),           DE("Beenden"),      FR("Quitter"),      ES("Salir"),
    PT("Sair"),          IT("Esci"),         NL("Afsluiten"),    RU("Выход"),
    TR("Çıkış"));

SS_MSG(menu_show_log,
    EN("Show Log Panel"),
    JA("ログパネルを表示"),
    ZH_HANS("显示日志面板"),
    ZH_HANT("顯示紀錄面板"),
    KO("로그 패널 표시"),
    DE("Protokollbereich anzeigen"),
    FR("Afficher le journal"),
    ES("Mostrar el panel de registro"),
    PT("Mostrar o painel de registro"),
    IT("Mostra il pannello di log"),
    NL("Logvenster tonen"),
    RU("Показать панель журнала"),
    TR("Günlük panelini göster"));

SS_MSG(menu_show_settings,
    EN("Show Settings Panel"),
    JA("設定パネルを表示"),
    ZH_HANS("显示设置面板"),
    ZH_HANT("顯示設定面板"),
    KO("설정 패널 표시"),
    DE("Einstellungsbereich anzeigen"),
    FR("Afficher le panneau des réglages"),
    ES("Mostrar el panel de ajustes"),
    PT("Mostrar o painel de configurações"),
    IT("Mostra il pannello delle impostazioni"),
    NL("Instellingenvenster tonen"),
    RU("Показать панель настроек"),
    TR("Ayarlar panelini göster"));

SS_MSG(menu_ui_size,
    EN("Interface Size"),
    JA("表示サイズ"),
    ZH_HANS("界面大小"),
    ZH_HANT("介面大小"),
    KO("인터페이스 크기"),
    DE("Anzeigegröße"),
    FR("Taille de l'interface"),
    ES("Tamaño de la interfaz"),
    PT("Tamanho da interface"),
    IT("Dimensione dell'interfaccia"),
    NL("Weergavegrootte"),
    RU("Размер интерфейса"),
    TR("Arayüz boyutu"));

SS_MSG(ui_size_auto,
    EN("Automatic (follow the window)"),
    JA("自動（ウィンドウに合わせる）"),
    ZH_HANS("自动（随窗口大小）"),
    ZH_HANT("自動（隨視窗大小）"),
    KO("자동(창 크기에 맞춤)"),
    DE("Automatisch (dem Fenster folgen)"),
    FR("Automatique (selon la fenêtre)"),
    ES("Automático (según la ventana)"),
    PT("Automático (conforme a janela)"),
    IT("Automatica (segue la finestra)"),
    NL("Automatisch (volgt het venster)"),
    RU("Автоматически (по размеру окна)"),
    TR("Otomatik (pencereye uy)"));

SS_MSG(menu_reset_layout,
    EN("Reset Panel Sizes"),
    JA("パネルの大きさを元に戻す"),
    ZH_HANS("重置面板大小"),
    ZH_HANT("重設面板大小"),
    KO("패널 크기 초기화"),
    DE("Bereichsgrößen zurücksetzen"),
    FR("Réinitialiser la taille des panneaux"),
    ES("Restablecer el tamaño de los paneles"),
    PT("Redefinir o tamanho dos painéis"),
    IT("Ripristina le dimensioni dei pannelli"),
    NL("Venstergroottes herstellen"),
    RU("Сбросить размеры панелей"),
    TR("Panel boyutlarını sıfırla"));

SS_MSG(menu_native_dialogs,
    EN("Use System File Dialogs"),
    JA("システムのファイル選択画面を使う"),
    ZH_HANS("使用系统文件对话框"),
    ZH_HANT("使用系統檔案對話框"),
    KO("시스템 파일 대화상자 사용"),
    DE("Dateidialoge des Systems verwenden"),
    FR("Utiliser les dialogues de fichiers du système"),
    ES("Usar los diálogos de archivos del sistema"),
    PT("Usar as caixas de diálogo de arquivo do sistema"),
    IT("Usa le finestre file del sistema"),
    NL("Bestandsvensters van het systeem gebruiken"),
    RU("Использовать системные диалоги файлов"),
    TR("Sistemin dosya pencerelerini kullan"));

SS_MSG(native_dialogs_help,
    EN("Pick files with the desktop's own file browser. Turn this off to use the simple built-in one, which is the only option on a session where no system dialog can be reached."),
    JA("ファイル選択にデスクトップ標準のファイルブラウザを使います。オフにすると内蔵の簡易ブラウザを使います。システムの選択画面を開けない環境では、そちらだけが使えます。"),
    ZH_HANS("用桌面自带的文件浏览器来选择文件。关闭后使用内置的简易浏览器；在无法调用系统对话框的环境里，内置的是唯一选择。"),
    ZH_HANT("用桌面自帶的檔案瀏覽器來選擇檔案。關閉後使用內建的簡易瀏覽器；在無法呼叫系統對話框的環境裡，內建的是唯一選擇。"),
    KO("바탕화면 환경의 파일 탐색기로 파일을 고릅니다. 끄면 내장된 간단한 탐색기를 씁니다. 시스템 대화상자를 열 수 없는 환경에서는 내장 쪽만 쓸 수 있습니다."),
    DE("Dateien mit dem Systemdialog wählen. Aus schaltet auf den eingebauten Browser um, auch wenn kein Systemdialog verfügbar ist."),
    FR("Choisir les fichiers avec le dialogue système. Désactiver pour le navigateur intégré, utilisable sans dialogue système."),
    ES("Elige archivos con el diálogo del sistema. Desactiva para usar el navegador integrado, disponible sin diálogos del sistema."),
    PT("Escolhe ficheiros pelo diálogo do sistema. Desative para usar o navegador interno, disponível sem diálogos do sistema."),
    IT("Scegli i file con il dialogo di sistema. Disattiva per il browser integrato, disponibile anche senza dialoghi di sistema."),
    NL("Kies bestanden met het systeemvenster. Uit gebruikt de ingebouwde browser, ook als er geen systeemvenster beschikbaar is."),
    RU("Выбирает файлы системным диалогом. Выключите для встроенного браузера, доступного и без системных диалогов."),
    TR("Dosyaları masaüstünün kendi dosya tarayıcısıyla seçin. Kapatınca yerleşik basit tarayıcı kullanılır; sistem penceresine erişilemeyen bir oturumda tek seçenek odur."));

SS_MSG(menu_open_dataset,
    EN("Open Dataset Folder..."),
    JA("データセットフォルダを開く…"),
    ZH_HANS("打开数据集文件夹…"),
    ZH_HANT("開啟資料集資料夾…"),
    KO("데이터셋 폴더 열기…"),
    DE("Datensatzordner öffnen …"),
    FR("Ouvrir un dossier de jeu de données…"),
    ES("Abrir una carpeta de conjunto de datos…"),
    PT("Abrir uma pasta de conjunto de dados…"),
    IT("Apri una cartella di set di dati…"),
    NL("Datasetmap openen…"),
    RU("Открыть папку набора данных…"),
    TR("Veri kümesi klasörü aç…"));

SS_MSG(menu_new_dataset,
    EN("New Dataset..."),
    JA("新しいデータセット…"),
    ZH_HANS("新建数据集…"),
    ZH_HANT("新建資料集…"),
    KO("새 데이터셋…"),
    DE("Neuer Datensatz …"),
    FR("Nouveau jeu de données…"),
    ES("Nuevo conjunto de datos…"),
    PT("Novo conjunto de dados…"),
    IT("Nuovo set di dati…"),
    NL("Nieuwe dataset…"),
    RU("Создать набор данных…"),
    TR("Yeni veri kümesi…"));

SS_MSG(menu_open_recent,
    EN("Open Recent"),
    JA("最近使った項目を開く"),
    ZH_HANS("打开最近使用的项目"),
    ZH_HANT("開啟最近使用的項目"),
    KO("최근 항목 열기"),
    DE("Zuletzt verwendet"),
    FR("Ouvrir un élément récent"),
    ES("Abrir recientes"),
    PT("Abrir recentes"),
    IT("Apri recenti"),
    NL("Recent geopend"),
    RU("Открыть недавние"),
    TR("Son kullanılanları aç"));

// ===========================================================================
// File dialog titles
// ===========================================================================

SS_MSG(pick_photo_folder,
    EN("Select Photo Folder"),
    JA("写真フォルダを選択"),
    ZH_HANS("选择照片文件夹"),
    ZH_HANT("選擇相片資料夾"),
    KO("사진 폴더 선택"),
    DE("Fotoordner wählen"),
    FR("Choisir un dossier de photos"),
    ES("Seleccionar una carpeta de fotos"),
    PT("Selecionar uma pasta de fotos"),
    IT("Seleziona una cartella di fotografie"),
    NL("Fotomap kiezen"),
    RU("Выбор папки с фотографиями"),
    TR("Fotoğraf klasörü seç"));

SS_MSG(pick_existing_dataset,
    EN("Select Dataset Folder"),
    JA("データセットフォルダを選択"),
    ZH_HANS("选择数据集文件夹"),
    ZH_HANT("選擇資料集資料夾"),
    KO("데이터셋 폴더 선택"),
    DE("Datensatzordner wählen"),
    FR("Choisir un dossier de jeu de données"),
    ES("Seleccionar una carpeta de conjunto de datos"),
    PT("Selecionar uma pasta de conjunto de dados"),
    IT("Seleziona una cartella di set di dati"),
    NL("Datasetmap kiezen"),
    RU("Выбор папки набора данных"),
    TR("Veri kümesi klasörü seç"));

SS_MSG(pick_videos,
    EN("Select Videos"),
    JA("動画を選択"),
    ZH_HANS("选择视频"),
    ZH_HANT("選擇影片"),
    KO("동영상 선택"),
    DE("Videos wählen"),
    FR("Choisir des vidéos"),
    ES("Seleccionar vídeos"),
    PT("Selecionar vídeos"),
    IT("Seleziona i video"),
    NL("Video's kiezen"),
    RU("Выбор видеофайлов"),
    TR("Video seç"));

SS_MSG(pick_video_file,
    EN("Select Video File"),
    JA("動画ファイルを選択"),
    ZH_HANS("选择视频文件"),
    ZH_HANT("選擇影片檔"),
    KO("동영상 파일 선택"),
    DE("Videodatei wählen"),
    FR("Choisir un fichier vidéo"),
    ES("Seleccionar un archivo de vídeo"),
    PT("Selecionar um arquivo de vídeo"),
    IT("Seleziona un file video"),
    NL("Videobestand kiezen"),
    RU("Выбор видеофайла"),
    TR("Video dosyası seç"));

SS_MSG(pick_output_folder,
    EN("Select Output Folder"),
    JA("出力フォルダを選択"),
    ZH_HANS("选择输出文件夹"),
    ZH_HANT("選擇輸出資料夾"),
    KO("출력 폴더 선택"),
    DE("Ausgabeordner wählen"),
    FR("Choisir un dossier de sortie"),
    ES("Seleccionar una carpeta de salida"),
    PT("Selecionar uma pasta de saída"),
    IT("Seleziona una cartella di destinazione"),
    NL("Uitvoermap kiezen"),
    RU("Выбор папки для результатов"),
    TR("Çıktı klasörü seç"));

SS_MSG(pick_vocab_tree,
    EN("Select Vocabulary Tree (.bin)"),
    JA("ボキャブラリツリー（.bin）を選択"),
    ZH_HANS("选择词汇树（.bin）"),
    ZH_HANT("選擇詞彙樹（.bin）"),
    KO("어휘 트리(.bin) 선택"),
    DE("Vokabularbaum (.bin) wählen"),
    FR("Choisir un arbre de vocabulaire (.bin)"),
    ES("Seleccionar un árbol de vocabulario (.bin)"),
    PT("Selecionar uma árvore de vocabulário (.bin)"),
    IT("Seleziona un albero di vocabolario (.bin)"),
    NL("Vocabulaireboom (.bin) kiezen"),
    RU("Выбор словарного дерева (.bin)"),
    TR("Sözcük ağacı (.bin) seç"));

// ===========================================================================
// Language picker
// ===========================================================================

// Shown under the language list when the FULL face is not installed. Not a
// warning: the interface and ordinary file names both render already, and what
// is left is the tail. {0} is the language's own name, {1} the size.
SS_MSG(font_needed,
    EN("Common {0} file names render already; a rare character can still show "
       "as a box until the full font is installed ({1})."),
    JA("よく使う{0}のファイル名はもう表示できます。まれな文字は、完全なフォント"
       "（{1}）を入れるまで四角で表示されることがあります。"),
    ZH_HANS("常用的{0}文件名已经可以显示；生僻字在安装完整字体（{1}）之前仍可能"
            "显示为方块。"),
    ZH_HANT("常用的{0}檔案名稱已經可以顯示；罕用字在安裝完整字型（{1}）之前仍"
            "可能顯示為方塊。"),
    KO("자주 쓰는 {0} 파일 이름은 이미 표시됩니다. 드문 글자는 전체 글꼴({1})을 "
       "설치하기 전까지 네모로 보일 수 있습니다."),
    DE("Gängige {0}-Dateinamen werden bereits angezeigt; ein seltenes Zeichen "
       "kann noch als Kästchen erscheinen, bis die vollständige Schriftart "
       "installiert ist ({1})."),
    FR("Les noms de fichiers en {0} courants s'affichent déjà ; un caractère "
       "rare peut encore apparaître en carré tant que la police complète "
       "n'est pas installée ({1})."),
    ES("Los nombres de archivo habituales en {0} ya se muestran; un carácter "
       "poco frecuente puede seguir apareciendo como recuadro hasta que "
       "instale la fuente completa ({1})."),
    PT("Os nomes de arquivo comuns em {0} já aparecem; um caractere raro ainda "
       "pode surgir como quadrado até instalar a fonte completa ({1})."),
    IT("I nomi di file in {0} più comuni vengono già visualizzati; un carattere "
       "raro può ancora apparire come rettangolo finché non installa il "
       "carattere completo ({1})."),
    NL("Gangbare bestandsnamen in het {0} worden al weergegeven; een zeldzaam "
       "teken kan nog als blokje verschijnen tot het volledige lettertype is "
       "geïnstalleerd ({1})."),
    RU("Обычные имена файлов на языке «{0}» уже отображаются; редкий символ "
       "может по-прежнему показываться прямоугольником, пока не установлен "
       "полный шрифт ({1})."),
    TR("Yaygın {0} dosya adları zaten görünüyor; ender bir karakter, tam yazı "
       "tipi kurulana kadar kutu olarak görünebilir ({1})."));

SS_MSG(font_download,
    EN("Install the full font"),
    JA("完全なフォントを入れる"),
    ZH_HANS("安装完整字体"),
    ZH_HANT("安裝完整字型"),
    KO("전체 글꼴 설치"),
    DE("Vollständige Schriftart installieren"),
    FR("Installer la police complète"),
    ES("Instalar la fuente completa"),
    PT("Instalar a fonte completa"),
    IT("Installa il carattere completo"),
    NL("Volledig lettertype installeren"),
    RU("Установить полный шрифт"),
    TR("Tam yazı tipini kur"));

SS_MSG(font_downloading,
    EN("Downloading the font..."),
    JA("フォントをダウンロードしています…"),
    ZH_HANS("正在下载字体…"),
    ZH_HANT("正在下載字型…"),
    KO("글꼴을 내려받는 중…"),
    DE("Schriftart wird heruntergeladen …"),
    FR("Téléchargement de la police…"),
    ES("Descargando la fuente…"),
    PT("Baixando a fonte…"),
    IT("Download del carattere in corso…"),
    NL("Lettertype wordt gedownload…"),
    RU("Загрузка шрифта…"),
    TR("Yazı tipi indiriliyor…"));

// {0} is whatever went wrong, from curl. Not translated -- it is a diagnostic.
SS_MSG(font_failed,
    EN("The font could not be downloaded: {0}"),
    JA("フォントをダウンロードできませんでした: {0}"),
    ZH_HANS("字体下载失败：{0}"),
    ZH_HANT("字型下載失敗：{0}"),
    KO("글꼴을 내려받지 못했습니다: {0}"),
    DE("Die Schriftart konnte nicht heruntergeladen werden: {0}"),
    FR("La police n'a pas pu être téléchargée : {0}"),
    ES("No se pudo descargar la fuente: {0}"),
    PT("Não foi possível baixar a fonte: {0}"),
    IT("Non è stato possibile scaricare il carattere: {0}"),
    NL("Het lettertype kon niet worden gedownload: {0}"),
    RU("Не удалось загрузить шрифт: {0}"),
    TR("Yazı tipi indirilemedi: {0}"));

SS_MSG(font_no_fetch,
    EN("This build cannot download fonts. Put the font file in the `fonts` "
       "folder beside the program, or set SS_FONT_DIR to where it is."),
    JA("このビルドはフォントをダウンロードできません。プログラムと同じ場所の "
       "`fonts` フォルダに置くか、SS_FONT_DIR で場所を指定してください。"),
    ZH_HANS("此版本无法下载字体。请把字体文件放到程序旁边的 `fonts` 文件夹，"
            "或用 SS_FONT_DIR 指定它所在的位置。"),
    ZH_HANT("此版本無法下載字型。請把字型檔放到程式旁邊的 `fonts` 資料夾，"
            "或用 SS_FONT_DIR 指定它所在的位置。"),
    KO("이 빌드는 글꼴을 내려받을 수 없습니다. 프로그램 옆의 `fonts` 폴더에 "
       "글꼴 파일을 두거나 SS_FONT_DIR로 위치를 지정하세요."),
    DE("Dieser Build kann keine Schriftarten herunterladen. Legen Sie die "
       "Schriftdatei in den Ordner `fonts` neben dem Programm, oder setzen "
       "Sie SS_FONT_DIR auf ihren Ort."),
    FR("Cette version ne peut pas télécharger de polices. Placez le fichier "
       "dans le dossier `fonts` à côté du programme, ou indiquez son "
       "emplacement avec SS_FONT_DIR."),
    ES("Esta compilación no puede descargar fuentes. Coloque el archivo en la "
       "carpeta `fonts` junto al programa, o indique su ubicación con "
       "SS_FONT_DIR."),
    PT("Esta compilação não pode baixar fontes. Coloque o arquivo na pasta "
       "`fonts` ao lado do programa, ou indique onde ele está com "
       "SS_FONT_DIR."),
    IT("Questa build non può scaricare caratteri. Metta il file nella "
       "cartella `fonts` accanto al programma, oppure indichi dove si trova "
       "con SS_FONT_DIR."),
    NL("Deze build kan geen lettertypen downloaden. Zet het bestand in de map "
       "`fonts` naast het programma, of wijs met SS_FONT_DIR aan waar het "
       "staat."),
    RU("Эта сборка не может загружать шрифты. Поместите файл шрифта в папку "
       "`fonts` рядом с программой или укажите путь к нему в SS_FONT_DIR."),
    TR("Bu sürüm yazı tipi indiremez. Yazı tipi dosyasını programın yanındaki "
       "`fonts` klasörüne koyun veya yerini SS_FONT_DIR ile belirtin."));

// ===========================================================================
// Home screen
// ===========================================================================

SS_MSG(home_back_to_training,
    EN("Back to Training"),   JA("学習に戻る"),
    ZH_HANS("返回训练"),       ZH_HANT("返回訓練"),
    KO("학습으로 돌아가기"),    DE("Zurück zum Training"),
    FR("Retour à l'entraînement"), ES("Volver al entrenamiento"),
    PT("Voltar ao treinamento"),   IT("Torna all'addestramento"),
    NL("Terug naar de training"),  RU("Вернуться к обучению"),
    TR("Eğitime dön"));

SS_MSG(home_back_to_trainer,
    EN("Back to Trainer"),    JA("トレーナーに戻る"),
    ZH_HANS("返回训练器"),      ZH_HANT("返回訓練器"),
    KO("트레이너로 돌아가기"),   DE("Zurück zum Trainer"),
    FR("Retour à l'atelier"), ES("Volver al entrenador"),
    PT("Voltar ao treinador"), IT("Torna all'addestratore"),
    NL("Terug naar de trainer"), RU("Вернуться к тренажёру"),
    TR("Eğiticiye dön"));

SS_MSG(home_open_dataset,
    EN("Open a Dataset..."),
    JA("データセットを開く…"),
    ZH_HANS("打开数据集…"),
    ZH_HANT("開啟資料集…"),
    KO("데이터셋 열기…"),
    DE("Datensatz öffnen …"),
    FR("Ouvrir un jeu de données…"),
    ES("Abrir un conjunto de datos…"),
    PT("Abrir um conjunto de dados…"),
    IT("Apri un set di dati…"),
    NL("Dataset openen…"),
    RU("Открыть набор данных…"),
    TR("Veri kümesi aç…"));

SS_MSG(home_open_dataset_help,
    EN("A folder containing an already-processed dataset: COLMAP (sparse/0), "
       "Nerfstudio (transforms.json), or Metashape exports. The format is "
       "detected automatically."),
    JA("処理済みのデータセットが入ったフォルダです。COLMAP（sparse/0）、"
       "Nerfstudio（transforms.json）、Metashape のエクスポートに対応し、"
       "形式は自動で判別されます。"),
    ZH_HANS("包含已处理数据集的文件夹：COLMAP（sparse/0）、"
            "Nerfstudio（transforms.json）或 Metashape 导出。格式会自动识别。"),
    ZH_HANT("包含已處理資料集的資料夾：COLMAP（sparse/0）、"
            "Nerfstudio（transforms.json）或 Metashape 匯出。格式會自動辨識。"),
    KO("이미 처리된 데이터셋이 들어 있는 폴더입니다. COLMAP(sparse/0), "
       "Nerfstudio(transforms.json), Metashape 내보내기를 지원하며 형식은 "
       "자동으로 인식됩니다."),
    DE("Ein Ordner mit einem bereits aufbereiteten Datensatz: COLMAP "
       "(sparse/0), Nerfstudio (transforms.json) oder Metashape-Exporte. Das "
       "Format wird automatisch erkannt."),
    FR("Un dossier contenant un jeu de données déjà traité : COLMAP "
       "(sparse/0), Nerfstudio (transforms.json) ou un export Metashape. Le "
       "format est détecté automatiquement."),
    ES("Una carpeta con un conjunto de datos ya procesado: COLMAP (sparse/0), "
       "Nerfstudio (transforms.json) o exportaciones de Metashape. El formato "
       "se detecta automáticamente."),
    PT("Uma pasta com um conjunto de dados já processado: COLMAP (sparse/0), "
       "Nerfstudio (transforms.json) ou exportações do Metashape. O formato é "
       "detectado automaticamente."),
    IT("Una cartella con un set di dati già elaborato: COLMAP (sparse/0), "
       "Nerfstudio (transforms.json) o esportazioni di Metashape. Il formato "
       "viene riconosciuto automaticamente."),
    NL("Een map met een al verwerkte dataset: COLMAP (sparse/0), Nerfstudio "
       "(transforms.json) of Metashape-exports. Het formaat wordt automatisch "
       "herkend."),
    RU("Папка с уже подготовленным набором данных: COLMAP (sparse/0), "
       "Nerfstudio (transforms.json) или экспорт из Metashape. Формат "
       "определяется автоматически."),
    TR("Halihazırda işlenmiş bir veri kümesi içeren klasör: COLMAP "
       "(sparse/0), Nerfstudio (transforms.json) veya Metashape dışa "
       "aktarımları. Biçim kendiliğinden algılanır."));

SS_MSG(home_new_dataset,
    EN("Create Dataset from Photos or Video..."),
    JA("写真や動画からデータセットを作成…"),
    ZH_HANS("从照片或视频创建数据集…"),
    ZH_HANT("從相片或影片建立資料集…"),
    KO("사진이나 동영상으로 데이터셋 만들기…"),
    DE("Datensatz aus Fotos oder Video erstellen …"),
    FR("Créer un jeu de données à partir de photos ou d'une vidéo…"),
    ES("Crear un conjunto de datos a partir de fotos o vídeo…"),
    PT("Criar um conjunto de dados a partir de fotos ou vídeo…"),
    IT("Crea un set di dati da fotografie o video…"),
    NL("Dataset maken uit foto's of video…"),
    RU("Создать набор данных из фотографий или видео…"),
    TR("Fotoğraflardan veya videodan veri kümesi oluştur…"));

SS_MSG(home_new_dataset_help,
    EN("Build a training dataset from photos, videos or both. Estimates camera positions and opens the trainer."),
    JA("写真、動画、または両方から学習用データセットを作成します。カメラ位置を推定し、トレーナーで開きます。"),
    ZH_HANS("用照片、视频或两者创建训练数据集。估算相机位置并打开训练器。"),
    ZH_HANT("用照片、影片或兩者建立訓練資料集。估算相機位置並開啟訓練器。"),
    KO("사진, 동영상 또는 둘 다로 학습 데이터셋을 만듭니다. 카메라 위치를 추정하고 학습 화면을 엽니다."),
    DE("Trainingsdatensatz aus Fotos, Videos oder beidem erstellen. Schätzt Kamerapositionen und öffnet den Trainer."),
    FR("Créer des données d’entraînement à partir de photos, vidéos ou des deux. Estime les caméras et ouvre l’entraînement."),
    ES("Crea datos de entrenamiento con fotos, vídeos o ambos. Estima las posiciones de cámara y abre el entrenador."),
    PT("Cria dados de treino com fotos, vídeos ou ambos. Estima as posições das câmaras e abre o treinador."),
    IT("Crea dati di addestramento da foto, video o entrambi. Stima le posizioni delle camere e apre il trainer."),
    NL("Maak trainingsdata van foto’s, video’s of beide. Schat cameraposities en opent de trainer."),
    RU("Создаёт данные для обучения из фото, видео или обоих. Оценивает позиции камер и открывает обучение."),
    TR("Fotoğraf, video veya ikisinden eğitim verisi oluşturur. Kamera konumlarını tahmin eder ve eğiticiyi açar."));

SS_MSG(home_drop_hint,
    EN("...or drop a dataset folder, photo folders, video files, a model or mesh file, or a camera project anywhere in this window"),
    JA("…または、データセットフォルダ・写真フォルダ・動画ファイル・モデルやメッシュのファイル・カメラプロジェクトをこのウィンドウのどこかにドロップしてください"),
    ZH_HANS("…或者把数据集文件夹、照片文件夹、视频文件、模型和网格文件，或者相机项目拖到这个窗口的任意位置"),
    ZH_HANT("…或者把資料集資料夾、相片資料夾、影片檔、模型和網格檔案，或者相機專案拖到這個視窗的任意位置"),
    KO("…또는 데이터셋 폴더, 사진 폴더, 동영상 파일, 모델이나 메시 파일, 카메라 프로젝트를 이 창 아무 곳에나 끌어다 놓으세요"),
    DE("… oder ziehen Sie einen Datensatzordner, Fotoordner, Videodateien, eine Modell- oder Netzdatei oder ein Kameraprojekt irgendwo in dieses Fenster"),
    FR("… ou déposez ici dossiers de données ou de photos, vidéos, modèles, maillages ou projets de caméra"),
    ES("… o arrastre una carpeta de conjunto de datos, carpetas de fotos, archivos de vídeo, un archivo de modelo o de malla, o un proyecto de cámara a cualquier punto de esta ventana"),
    PT("… ou arraste uma pasta de conjunto de dados, pastas de fotos, arquivos de vídeo, um arquivo de modelo ou de malha, ou um projeto de câmera para qualquer ponto desta janela"),
    IT("… oppure trascina una cartella di dataset, cartelle di foto, file video, un file di modello o di mesh, o un progetto di camera in un punto qualsiasi di questa finestra"),
    NL("… of sleep een datasetmap, fotomappen, videobestanden, een model- of meshbestand, of een cameraproject ergens in dit venster"),
    RU("…или перетащите папку набора данных, папки с фотографиями, видеофайлы, файл модели или меша либо проект камеры в любое место этого окна"),
    TR("…ya da bir veri kümesi klasörünü, fotoğraf klasörlerini, video dosyalarını, bir model ya da ağ dosyasını veya bir kamera projesini bu pencerenin herhangi bir yerine bırakın"));

SS_MSG(home_recent,
    EN("Recent"),        JA("最近使った項目"), ZH_HANS("最近"),   ZH_HANT("最近"),
    KO("최근 항목"),      DE("Zuletzt"),      FR("Récents"),     ES("Recientes"),
    PT("Recentes"),      IT("Recenti"),      NL("Recent"),      RU("Недавние"),
    TR("Son kullanılan"));

// The recent list's tabs: "Recent" (home_recent) holds every kind, these one
// kind each. Labels, so plurals are fine.
SS_MSG(home_tab_datasets,
    EN("Datasets"),      JA("データセット"),   ZH_HANS("数据集"),   ZH_HANT("資料集"),
    KO("데이터셋"),       DE("Datensätze"),   FR("Jeux de données"),
    ES("Conjuntos de datos"), PT("Conjuntos de dados"), IT("Set di dati"),
    NL("Datasets"),      RU("Наборы данных"), TR("Veri kümeleri"));

SS_MSG(home_tab_recons,
    EN("Reconstructions"), JA("再構成"),     ZH_HANS("重建"),     ZH_HANT("重建"),
    KO("재구성"),         DE("Rekonstruktionen"), FR("Reconstructions"),
    ES("Reconstrucciones"), PT("Reconstruções"), IT("Ricostruzioni"),
    NL("Reconstructies"), RU("Реконструкции"), TR("Yeniden kurmalar"));

SS_MSG(home_tab_models,
    EN("Models"),        JA("モデル"),        ZH_HANS("模型"),     ZH_HANT("模型"),
    KO("모델"),           DE("Modelle"),      FR("Modèles"),      ES("Modelos"),
    PT("Modelos"),       IT("Modelli"),      NL("Modellen"),     RU("Модели"),
    TR("Modeller"));

SS_MSG(home_tab_runs,
    EN("Training runs"), JA("学習の実行"),     ZH_HANS("训练运行"), ZH_HANT("訓練執行"),
    KO("학습 실행"),      DE("Trainingsläufe"), FR("Entraînements"),
    ES("Entrenamientos"), PT("Treinamentos"), IT("Addestramenti"),
    NL("Trainingsruns"), RU("Запуски обучения"), TR("Eğitimler"));

SS_MSG(home_tab_projects,
    EN("Camera projects"), JA("カメラプロジェクト"), ZH_HANS("相机项目"),
    ZH_HANT("相機專案"),   KO("카메라 프로젝트"), DE("Kameraprojekte"),
    FR("Projets de caméra"), ES("Proyectos de cámara"),
    PT("Projetos de câmera"), IT("Progetti di camera"), NL("Cameraprojecten"),
    RU("Проекты камеры"), TR("Kamera projeleri"));

// What one entry is, beside it in the "Recent" tab. A model says what the
// file turned out to hold once that is known, and "Model" until then.
SS_MSG(home_kind_dataset,
    EN("Dataset"),       JA("データセット"),   ZH_HANS("数据集"),   ZH_HANT("資料集"),
    KO("데이터셋"),       DE("Datensatz"),    FR("Jeu de données"),
    ES("Conjunto de datos"), PT("Conjunto de dados"), IT("Set di dati"),
    NL("Dataset"),       RU("Набор данных"), TR("Veri kümesi"));

SS_MSG(home_kind_recon,
    EN("Reconstruction"), JA("再構成"),      ZH_HANS("重建"),     ZH_HANT("重建"),
    KO("재구성"),         DE("Rekonstruktion"), FR("Reconstruction"),
    ES("Reconstrucción"), PT("Reconstrução"), IT("Ricostruzione"),
    NL("Reconstructie"), RU("Реконструкция"), TR("Yeniden kurma"));

SS_MSG(home_kind_model,
    EN("Model"),         JA("モデル"),        ZH_HANS("模型"),     ZH_HANT("模型"),
    KO("모델"),           DE("Modell"),       FR("Modèle"),       ES("Modelo"),
    PT("Modelo"),        IT("Modello"),      NL("Model"),        RU("Модель"),
    TR("Model"));

SS_MSG(home_kind_splats,
    EN("Splats"),        JA("スプラット"),     ZH_HANS("高斯点"),   ZH_HANT("高斯點"),
    KO("스플랫"),         DE("Splats"),       FR("Splats"),       ES("Splats"),
    PT("Splats"),        IT("Splat"),        NL("Splats"),       RU("Сплаты"),
    TR("Splat'lar"));

SS_MSG(home_kind_mesh,
    EN("Mesh"),          JA("メッシュ"),       ZH_HANS("网格"),     ZH_HANT("網格"),
    KO("메시"),           DE("Netz"),         FR("Maillage"),     ES("Malla"),
    PT("Malha"),         IT("Mesh"),         NL("Mesh"),         RU("Меш"),
    TR("Ağ"));

SS_MSG(home_kind_points,
    EN("Point cloud"),   JA("点群"),          ZH_HANS("点云"),     ZH_HANT("點雲"),
    KO("포인트 클라우드"), DE("Punktwolke"),   FR("Nuage de points"),
    ES("Nube de puntos"), PT("Nuvem de pontos"), IT("Nuvola di punti"),
    NL("Puntenwolk"),    RU("Облако точек"), TR("Nokta bulutu"));

SS_MSG(home_kind_run,
    EN("Training run"),  JA("学習の実行"),     ZH_HANS("训练运行"), ZH_HANT("訓練執行"),
    KO("학습 실행"),      DE("Trainingslauf"), FR("Entraînement"),
    ES("Entrenamiento"), PT("Treinamento"),  IT("Addestramento"),
    NL("Trainingsrun"),  RU("Запуск обучения"), TR("Eğitim"));

SS_MSG(home_kind_project,
    EN("Camera project"), JA("カメラプロジェクト"), ZH_HANS("相机项目"),
    ZH_HANT("相機專案"),   KO("카메라 프로젝트"), DE("Kameraprojekt"),
    FR("Projet de caméra"), ES("Proyecto de cámara"), PT("Projeto de câmera"),
    IT("Progetto di camera"), NL("Cameraproject"), RU("Проект камеры"),
    TR("Kamera projesi"));

// What each tab holds: its hover help, and what an empty one says.
SS_MSG(home_recent_about,
    EN("Everything opened, built or trained here lately, newest first. Whatever is no longer on disk drops off the list by itself. Right-click an entry for more."),
    JA("最近ここで開いた・作成した・学習したものが新しい順に並びます。ディスクから消えたものは自動的に一覧から外れます。項目を右クリックすると、ほかの操作も選べます。"),
    ZH_HANS("最近在这里打开、创建或训练过的内容，按从新到旧排列。已不在磁盘上的条目会自动从列表中移除。右键单击条目可进行更多操作。"),
    ZH_HANT("最近在這裡開啟、建立或訓練過的內容，按從新到舊排列。已不在磁碟上的項目會自動從清單中移除。在項目上按右鍵可進行更多操作。"),
    KO("최근 여기서 열거나 만들거나 학습한 항목이 최신순으로 표시됩니다. 디스크에서 사라진 항목은 목록에서 저절로 빠집니다.\n항목을 마우스 오른쪽 버튼으로 클릭하면 다른 작업도 할 수 있습니다."),
    DE("Alles, was hier zuletzt geöffnet, erstellt oder trainiert wurde, das Neueste zuerst.\n- Was nicht mehr auf der Festplatte liegt, verschwindet von selbst aus der Liste.\n- Ein Rechtsklick auf einen Eintrag bietet mehr."),
    FR("Tout ce qui a été ouvert, créé ou entraîné ici récemment, du plus récent au plus ancien.\nCe qui n'est plus sur le disque quitte la liste tout seul. Un clic droit sur une entrée propose d'autres actions."),
    ES("Todo lo que se abrió, creó o entrenó aquí últimamente, de lo más reciente a lo más antiguo.\nLo que ya no está en el disco sale de la lista por sí solo. Haga clic derecho en una entrada para ver más."),
    PT("Tudo o que foi aberto, criado ou treinado aqui recentemente, do mais novo ao mais antigo.\nO que não está mais no disco sai da lista sozinho. Clique com o botão direito em uma entrada para ver mais."),
    IT("Tutto ciò che è stato aperto, creato o addestrato qui di recente, dal più nuovo.\nCiò che non è più sul disco esce dall'elenco da solo. Fai clic destro su una voce per altre azioni."),
    NL("Alles wat hier onlangs is geopend, gemaakt of getraind, het nieuwste eerst.\nWat niet meer op de schijf staat, verdwijnt vanzelf uit de lijst. Klik met de rechtermuisknop op een item voor meer."),
    RU("Всё, что недавно открывалось, создавалось или обучалось здесь, — сначала самое новое.\n- То, чего больше нет на диске, само исчезает из списка.\n- Щёлкните запись правой кнопкой мыши, чтобы увидеть другие действия."),
    TR("Burada son zamanlarda açılan, oluşturulan ya da eğitilen her şey, en yenisi önce.\nArtık diskte olmayanlar listeden kendiliğinden düşer. Diğer işlemler için bir girdiye sağ tıklayın."));

SS_MSG(home_datasets_about,
    EN("Datasets opened in the trainer. Click one to open it there again."),
    JA("トレーナーで開いたデータセットです。クリックすると、もう一度トレーナーで"
       "開きます。"),
    ZH_HANS("在训练器中打开过的数据集。单击即可在训练器中再次打开。"),
    ZH_HANT("在訓練器中開啟過的資料集。按一下即可在訓練器中再次開啟。"),
    KO("트레이너에서 열었던 데이터셋입니다. 클릭하면 트레이너에서 다시 엽니다."),
    DE("Im Trainer geöffnete Datensätze. Ein Klick öffnet einen davon dort "
       "erneut."),
    FR("Les jeux de données ouverts dans l'atelier. Cliquez sur l'un d'eux pour "
       "l'y rouvrir."),
    ES("Conjuntos de datos abiertos en el entrenador. Haga clic en uno para "
       "volver a abrirlo allí."),
    PT("Conjuntos de dados abertos no treinador. Clique em um para abri-lo lá "
       "de novo."),
    IT("Set di dati aperti nell'addestratore. Fai clic su uno per riaprirlo lì."),
    NL("Datasets die in de trainer zijn geopend. Klik op een dataset om hem daar "
       "opnieuw te openen."),
    RU("Наборы данных, открытые в тренажёре. Щёлкните по одному, чтобы снова "
       "открыть его там."),
    TR("Eğiticide açılan veri kümeleri. Birine tıklayınca orada yeniden açılır."));

SS_MSG(home_recons_about,
    EN("Datasets built here from photos or video. Click one to go back to the "
       "screen that built it, with its inputs and settings."),
    JA("ここで写真や動画から作成したデータセットです。クリックすると、入力と"
       "設定をそのままに、作成した画面に戻ります。"),
    ZH_HANS("在这里从照片或视频创建的数据集。单击即可回到创建它的界面，输入和"
            "设置都会恢复。"),
    ZH_HANT("在這裡從相片或影片建立的資料集。按一下即可回到建立它的畫面，輸入和"
            "設定都會還原。"),
    KO("여기서 사진이나 동영상으로 만든 데이터셋입니다. 클릭하면 입력과 설정을 "
       "그대로 가지고 그것을 만든 화면으로 돌아갑니다."),
    DE("Hier aus Fotos oder Video erstellte Datensätze. Ein Klick führt mit "
       "Eingaben und Einstellungen zurück zu dem Bildschirm, der ihn erstellt "
       "hat."),
    FR("Les jeux de données créés ici à partir de photos ou de vidéo. Cliquez "
       "sur l'un d'eux pour revenir à l'écran qui l'a créé, avec ses entrées et "
       "ses réglages."),
    ES("Conjuntos de datos creados aquí a partir de fotos o vídeo. Haga clic en "
       "uno para volver a la pantalla que lo creó, con sus entradas y ajustes."),
    PT("Conjuntos de dados criados aqui a partir de fotos ou vídeo. Clique em "
       "um para voltar à tela que o criou, com suas entradas e configurações."),
    IT("Set di dati creati qui da fotografie o video. Fai clic su uno per "
       "tornare alla schermata che l'ha creato, con i suoi ingressi e le sue "
       "impostazioni."),
    NL("Datasets die hier uit foto's of video zijn gemaakt. Klik op een dataset "
       "om terug te gaan naar het scherm dat hem maakte, met zijn invoer en "
       "instellingen."),
    RU("Наборы данных, созданные здесь из фотографий или видео. Щёлкните по "
       "одному, чтобы вернуться на экран, где он создавался, со всеми входами и "
       "настройками."),
    TR("Burada fotoğraflardan veya videodan oluşturulan veri kümeleri. Birine "
       "tıklayınca, girdileri ve ayarlarıyla birlikte onu oluşturan ekrana "
       "dönülür."));

SS_MSG(home_models_about,
    EN("Splats, meshes, point clouds and reconstructions opened in the viewer."),
    JA("ビューアで開いたスプラット、メッシュ、点群、再構成です。"),
    ZH_HANS("在查看器中打开过的高斯点、网格、点云和重建。"),
    ZH_HANT("在檢視器中開啟過的高斯點、網格、點雲和重建。"),
    KO("뷰어에서 열었던 스플랫, 메시, 포인트 클라우드, 재구성입니다."),
    DE("Im Betrachter geöffnete Splats, Netze, Punktwolken und "
       "Rekonstruktionen."),
    FR("Les splats, maillages, nuages de points et reconstructions ouverts dans "
       "la visionneuse."),
    ES("Splats, mallas, nubes de puntos y reconstrucciones abiertos en el visor."),
    PT("Splats, malhas, nuvens de pontos e reconstruções abertos no "
       "visualizador."),
    IT("Splat, mesh, nuvole di punti e ricostruzioni aperti nel visualizzatore."),
    NL("Splats, meshes, puntenwolken en reconstructies die in de viewer zijn "
       "geopend."),
    RU("Сплаты, меши, облака точек и реконструкции, открытые в просмотрщике."),
    TR("Görüntüleyicide açılan splat'lar, ağlar, nokta bulutları ve yeniden "
       "yapımlar."));

SS_MSG(home_runs_about,
    EN("Training runs that finished and saved a model. Click one to look at it "
       "in the viewer."),
    JA("最後まで進んでモデルを保存した学習です。クリックするとビューアで表示"
       "します。"),
    ZH_HANS("已完成并保存了模型的训练运行。单击即可在查看器中查看。"),
    ZH_HANT("已完成並儲存了模型的訓練執行。按一下即可在檢視器中檢視。"),
    KO("끝까지 진행되어 모델을 저장한 학습 실행입니다. 클릭하면 뷰어에서 "
       "봅니다."),
    DE("Trainingsläufe, die fertig wurden und ein Modell gespeichert haben. Ein "
       "Klick zeigt es im Betrachter."),
    FR("Les entraînements terminés qui ont enregistré un modèle. Cliquez sur "
       "l'un d'eux pour le voir dans la visionneuse."),
    ES("Entrenamientos que terminaron y guardaron un modelo. Haga clic en uno "
       "para verlo en el visor."),
    PT("Treinamentos que terminaram e salvaram um modelo. Clique em um para "
       "vê-lo no visualizador."),
    IT("Addestramenti conclusi che hanno salvato un modello. Fai clic su uno "
       "per vederlo nel visualizzatore."),
    NL("Trainingsruns die klaar zijn en een model hebben opgeslagen. Klik op een "
       "run om het model in de viewer te bekijken."),
    RU("Завершённые запуски обучения, сохранившие модель. Щёлкните по одному, "
       "чтобы посмотреть её в просмотрщике."),
    TR("Biten ve bir model kaydeden eğitimler. Birine tıklayınca "
       "görüntüleyicide açılır."));

SS_MSG(home_projects_about,
    EN("Camera moves saved or opened for a photo or video. Click one to open it "
       "with its model."),
    JA("写真や動画のために保存した、または開いたカメラの動きです。クリックすると"
       "モデルと一緒に開きます。"),
    ZH_HANS("为照片或视频保存或打开过的相机运动。单击即可连同其模型一起打开。"),
    ZH_HANT("為相片或影片儲存或開啟過的相機運動。按一下即可連同其模型一起開啟。"),
    KO("사진이나 동영상을 위해 저장하거나 열었던 카메라 움직임입니다. 클릭하면 "
       "그 모델과 함께 엽니다."),
    DE("Für ein Foto oder Video gespeicherte oder geöffnete Kamerafahrten. Ein "
       "Klick öffnet eine davon mit ihrem Modell."),
    FR("Les mouvements de caméra enregistrés ou ouverts pour une photo ou une "
       "vidéo. Cliquez sur l'un d'eux pour l'ouvrir avec son modèle."),
    ES("Movimientos de cámara guardados o abiertos para una foto o un vídeo. "
       "Haga clic en uno para abrirlo con su modelo."),
    PT("Movimentos de câmera salvos ou abertos para uma foto ou um vídeo. "
       "Clique em um para abri-lo com o seu modelo."),
    IT("Movimenti di camera salvati o aperti per una foto o un video. Fai clic "
       "su uno per aprirlo con il suo modello."),
    NL("Camerabewegingen die zijn opgeslagen of geopend voor een foto of video. "
       "Klik op een beweging om hem met zijn model te openen."),
    RU("Движения камеры, сохранённые или открытые для фото или видео. Щёлкните "
       "по одному, чтобы открыть его вместе с моделью."),
    TR("Bir fotoğraf ya da video için kaydedilen veya açılan kamera hareketleri. "
       "Birine tıklayınca modeliyle birlikte açılır."));

SS_MSG(home_recent_empty,
    EN("Nothing here yet."), JA("まだ何もありません。"),
    ZH_HANS("这里还没有内容。"), ZH_HANT("這裡還沒有內容。"),
    KO("아직 아무것도 없습니다."), DE("Hier ist noch nichts."),
    FR("Rien ici pour l'instant."), ES("Aquí aún no hay nada."),
    PT("Ainda não há nada aqui."), IT("Qui non c'è ancora niente."),
    NL("Hier staat nog niets."), RU("Здесь пока пусто."),
    TR("Burada henüz bir şey yok."));

// When an entry was last used: a time today or yesterday, a date before that.
SS_MSG(home_recent_today,
    EN("Today {0}"),     JA("今日 {0}"),      ZH_HANS("今天 {0}"), ZH_HANT("今天 {0}"),
    KO("오늘 {0}"),       DE("Heute {0}"),    FR("Aujourd'hui {0}"), ES("Hoy {0}"),
    PT("Hoje {0}"),      IT("Oggi {0}"),     NL("Vandaag {0}"),  RU("Сегодня {0}"),
    TR("Bugün {0}"));

SS_MSG(home_recent_yesterday,
    EN("Yesterday {0}"), JA("昨日 {0}"),      ZH_HANS("昨天 {0}"), ZH_HANT("昨天 {0}"),
    KO("어제 {0}"),       DE("Gestern {0}"),  FR("Hier {0}"),     ES("Ayer {0}"),
    PT("Ontem {0}"),     IT("Ieri {0}"),     NL("Gisteren {0}"), RU("Вчера {0}"),
    TR("Dün {0}"));

// An entry's right-click menu.
SS_MSG(home_recent_open,
    EN("Open"),          JA("開く"),          ZH_HANS("打开"),     ZH_HANT("開啟"),
    KO("열기"),           DE("Öffnen"),       FR("Ouvrir"),       ES("Abrir"),
    PT("Abrir"),         IT("Apri"),         NL("Openen"),       RU("Открыть"),
    TR("Aç"));

SS_MSG(home_recent_show,
    EN("Show in folder"), JA("フォルダで表示"), ZH_HANS("在文件夹中显示"),
    ZH_HANT("在資料夾中顯示"), KO("폴더에서 보기"), DE("Im Ordner anzeigen"),
    FR("Afficher dans le dossier"), ES("Mostrar en la carpeta"),
    PT("Mostrar na pasta"), IT("Mostra nella cartella"), NL("In map weergeven"),
    RU("Показать в папке"), TR("Klasörde göster"));

SS_MSG(home_recent_copy,
    EN("Copy path"),     JA("パスをコピー"),   ZH_HANS("复制路径"), ZH_HANT("複製路徑"),
    KO("경로 복사"),      DE("Pfad kopieren"), FR("Copier le chemin"),
    ES("Copiar la ruta"), PT("Copiar o caminho"), IT("Copia il percorso"),
    NL("Pad kopiëren"),  RU("Скопировать путь"), TR("Yolu kopyala"));

SS_MSG(home_recent_remove,
    EN("Remove from list"), JA("一覧から削除"), ZH_HANS("从列表中移除"),
    ZH_HANT("從清單中移除"), KO("목록에서 제거"), DE("Aus der Liste entfernen"),
    FR("Retirer de la liste"), ES("Quitar de la lista"), PT("Remover da lista"),
    IT("Rimuovi dall'elenco"), NL("Uit de lijst verwijderen"),
    RU("Убрать из списка"), TR("Listeden kaldır"));

SS_MSG(home_recent_clear,
    EN("Clear this list"), JA("この一覧を消去"), ZH_HANS("清空此列表"),
    ZH_HANT("清空此清單"), KO("이 목록 비우기"), DE("Diese Liste leeren"),
    FR("Vider cette liste"), ES("Vaciar esta lista"), PT("Limpar esta lista"),
    IT("Svuota questo elenco"), NL("Deze lijst wissen"),
    RU("Очистить этот список"), TR("Bu listeyi temizle"));

SS_MSG(home_recon_busy,
    EN("Something is still running. Let it finish, or stop it, before opening "
       "another reconstruction."),
    JA("まだ処理が実行中です。別の再構成を開く前に、完了を待つか停止して"
       "ください。"),
    ZH_HANS("仍有任务在运行。请等它完成或将其停止，然后再打开另一个重建。"),
    ZH_HANT("仍有工作在執行。請等它完成或將其停止，然後再開啟另一個重建。"),
    KO("아직 실행 중인 작업이 있습니다. 다른 재구성을 열기 전에 끝날 때까지 "
       "기다리거나 중지하세요."),
    DE("Es läuft noch etwas. Lassen Sie es fertig werden oder beenden Sie es, "
       "bevor Sie eine andere Rekonstruktion öffnen."),
    FR("Une tâche est encore en cours. Laissez-la se terminer ou arrêtez-la "
       "avant d'ouvrir une autre reconstruction."),
    ES("Todavía hay algo en marcha. Deje que termine, o deténgalo, antes de "
       "abrir otra reconstrucción."),
    PT("Ainda há algo em execução. Deixe terminar, ou interrompa, antes de "
       "abrir outra reconstrução."),
    IT("C'è ancora qualcosa in corso. Lascialo finire, o fermalo, prima di "
       "aprire un'altra ricostruzione."),
    NL("Er loopt nog iets. Laat het afronden of stop het voordat u een andere "
       "reconstructie opent."),
    RU("Что-то ещё выполняется. Дождитесь завершения или остановите задачу, "
       "прежде чем открывать другую реконструкцию."),
    TR("Hâlâ çalışan bir iş var. Başka bir yeniden kurmayı açmadan önce "
       "bitmesini bekleyin ya da durdurun."));

SS_MSG(home_no_engine,
    EN("note: neither the built-in reconstruction nor COLMAP was found, so "
       "datasets cannot be created here (training an existing one still "
       "works)."),
    JA("注意: 内蔵の再構成も COLMAP も見つからないため、ここではデータセットを"
       "作成できません（既存のデータセットの学習は行えます）。"),
    ZH_HANS("注意：既没有找到内置重建，也没有找到 COLMAP，因此无法在这里创建"
            "数据集（训练已有数据集仍然可用）。"),
    ZH_HANT("注意：既沒有找到內建重建，也沒有找到 COLMAP，因此無法在這裡建立"
            "資料集（訓練既有資料集仍然可用）。"),
    KO("참고: 내장 재구성도 COLMAP도 찾지 못해 여기서는 데이터셋을 만들 수 "
       "없습니다(기존 데이터셋 학습은 그대로 됩니다)."),
    DE("Hinweis: Weder die eingebaute Rekonstruktion noch COLMAP wurde "
       "gefunden, daher lassen sich hier keine Datensätze erstellen (einen "
       "vorhandenen zu trainieren geht weiterhin)."),
    FR("note : ni la reconstruction intégrée ni COLMAP n'ont été trouvées, "
       "les jeux de données ne peuvent donc pas être créés ici (entraîner un "
       "jeu existant fonctionne toujours)."),
    ES("nota: no se encontró ni la reconstrucción integrada ni COLMAP, así "
       "que aquí no se pueden crear conjuntos de datos (entrenar uno "
       "existente sigue funcionando)."),
    PT("nota: nem a reconstrução integrada nem o COLMAP foram encontrados, "
       "então não é possível criar conjuntos de dados aqui (treinar um "
       "existente continua funcionando)."),
    IT("nota: non sono stati trovati né la ricostruzione integrata né COLMAP, "
       "quindi qui non si possono creare set di dati (addestrarne uno "
       "esistente funziona ancora)."),
    NL("let op: noch de ingebouwde reconstructie noch COLMAP is gevonden, dus "
       "hier kunnen geen datasets worden gemaakt (een bestaande trainen werkt "
       "nog wel)."),
    RU("примечание: не найдены ни встроенная реконструкция, ни COLMAP, "
       "поэтому создать набор данных здесь нельзя (обучение на готовом "
       "по-прежнему работает)."),
    TR("not: ne yerleşik yeniden oluşturma ne de COLMAP bulundu, bu yüzden "
       "burada veri kümesi oluşturulamıyor (var olan bir kümeyi eğitmek yine "
       "de çalışır)."));

// ===========================================================================
// Train screen
// ===========================================================================

SS_MSG(back_home,
    EN("< Home"),        JA("< ホーム"),      ZH_HANS("< 主页"),  ZH_HANT("< 首頁"),
    KO("< 홈"),           DE("< Start"),      FR("< Accueil"),   ES("< Inicio"),
    PT("< Início"),      IT("< Home"),       NL("< Start"),     RU("< Главная"),
    TR("< Ana ekran"));

SS_MSG(leaving_stops_training,
    EN("Training is in progress -- leaving stops it (a checkpoint is saved "
       "first)."),
    JA("学習中です。ここを離れると学習は停止します（先にチェックポイントが"
       "保存されます）。"),
    ZH_HANS("正在训练。离开会停止训练（会先保存一个检查点）。"),
    ZH_HANT("正在訓練。離開會停止訓練（會先儲存一個檢查點）。"),
    KO("학습이 진행 중입니다. 여기를 떠나면 학습이 멈춥니다(먼저 체크포인트를 "
       "저장합니다)."),
    DE("Das Training läuft -- wer hier weggeht, beendet es (zuvor wird ein "
       "Prüfpunkt gespeichert)."),
    FR("L'entraînement est en cours : quitter l'arrête (un point de "
       "sauvegarde est enregistré avant)."),
    ES("El entrenamiento está en marcha: salir lo detiene (antes se guarda un "
       "punto de control)."),
    PT("O treinamento está em andamento: sair o interrompe (um ponto de "
       "verificação é salvo antes)."),
    IT("L'addestramento è in corso: uscire lo interrompe (prima viene salvato "
       "un punto di controllo)."),
    NL("De training loopt -- weggaan stopt hem (er wordt eerst een "
       "controlepunt opgeslagen)."),
    RU("Идёт обучение — уход отсюда его остановит (перед этим сохраняется "
       "контрольная точка)."),
    TR("Eğitim sürüyor -- buradan ayrılmak onu durdurur (önce bir denetim "
       "noktası kaydedilir)."));

SS_MSG(section_dataset,
    EN("Dataset"),       JA("データセット"),   ZH_HANS("数据集"),   ZH_HANT("資料集"),
    KO("데이터셋"),       DE("Datensatz"),    FR("Jeu de données"),
    ES("Conjunto de datos"), PT("Conjunto de dados"), IT("Set di dati"),
    NL("Dataset"),       RU("Набор данных"), TR("Veri kümesi"));

SS_MSG(section_device,
    EN("Device"),        JA("デバイス"),       ZH_HANS("设备"),     ZH_HANT("裝置"),
    KO("장치"),           DE("Gerät"),        FR("Périphérique"), ES("Dispositivo"),
    PT("Dispositivo"),   IT("Dispositivo"),  NL("Apparaat"),     RU("Устройство"),
    TR("Aygıt"));

SS_MSG(section_preset,
    EN("Preset"),        JA("プリセット"),     ZH_HANS("预设"),     ZH_HANT("預設"),
    KO("프리셋"),         DE("Voreinstellung"), FR("Préréglage"),  ES("Ajuste"),
    PT("Predefinição"),  IT("Preimpostazione"), NL("Voorinstelling"),
    RU("Пресет"),        TR("Hazır ayar"));

SS_MSG(section_basic_options,
    EN("Basic Options"),
    JA("基本設定"),        ZH_HANS("基本选项"),  ZH_HANT("基本選項"),
    KO("기본 옵션"),       DE("Grundeinstellungen"), FR("Options de base"),
    ES("Opciones básicas"), PT("Opções básicas"), IT("Opzioni di base"),
    NL("Basisopties"),   RU("Основные параметры"), TR("Temel seçenekler"));

SS_MSG(section_all_options,
    EN("All Options (Advanced)"),
    JA("すべての設定（詳細）"),
    ZH_HANS("全部选项（高级）"),
    ZH_HANT("全部選項（進階）"),
    KO("모든 옵션(고급)"),
    DE("Alle Einstellungen (erweitert)"),
    FR("Toutes les options (avancé)"),
    ES("Todas las opciones (avanzado)"),
    PT("Todas as opções (avançado)"),
    IT("Tutte le opzioni (avanzate)"),
    NL("Alle opties (geavanceerd)"),
    RU("Все параметры (дополнительно)"),
    TR("Tüm seçenekler (gelişmiş)"));

SS_MSG(section_training,
    EN("Training"),      JA("学習"),          ZH_HANS("训练"),     ZH_HANT("訓練"),
    KO("학습"),           DE("Training"),     FR("Entraînement"), ES("Entrenamiento"),
    PT("Treinamento"),   IT("Addestramento"), NL("Training"),    RU("Обучение"),
    TR("Eğitim"));

SS_MSG(section_metrics,
    EN("Metrics"),       JA("指標"),          ZH_HANS("指标"),     ZH_HANT("指標"),
    KO("지표"),           DE("Kennzahlen"),   FR("Mesures"),      ES("Métricas"),
    PT("Métricas"),      IT("Metriche"),     NL("Meetwaarden"),  RU("Метрики"),
    TR("Ölçümler"));

SS_MSG(parsing_dataset,
    EN("Parsing dataset ..."),
    JA("データセットを読み込んでいます…"),
    ZH_HANS("正在解析数据集…"),
    ZH_HANT("正在解析資料集…"),
    KO("데이터셋을 읽는 중…"),
    DE("Datensatz wird gelesen …"),
    FR("Lecture du jeu de données…"),
    ES("Analizando el conjunto de datos…"),
    PT("Analisando o conjunto de dados…"),
    IT("Lettura del set di dati…"),
    NL("Dataset wordt gelezen…"),
    RU("Разбор набора данных…"),
    TR("Veri kümesi okunuyor…"));

SS_MSG(no_dataset_loaded,
    EN("no dataset loaded"),
    JA("データセットが読み込まれていません"),
    ZH_HANS("未加载数据集"),
    ZH_HANT("未載入資料集"),
    KO("불러온 데이터셋 없음"),
    DE("kein Datensatz geladen"),
    FR("aucun jeu de données chargé"),
    ES("no hay ningún conjunto de datos cargado"),
    PT("nenhum conjunto de dados carregado"),
    IT("nessun set di dati caricato"),
    NL("geen dataset geladen"),
    RU("набор данных не загружен"),
    TR("yüklü veri kümesi yok"));

// {0} cameras, {1} views, {2} points ("1.2M"). Counts are labelled rather
// than inflected -- see the plural rule at the top of this file.
SS_MSG(dataset_summary,
    EN("Cameras: {0} ({1} views) - points: {2}"),
    JA("カメラ: {0}（ビュー {1}）- 点: {2}"),
    ZH_HANS("相机：{0}（视图 {1}）- 点：{2}"),
    ZH_HANT("相機：{0}（視圖 {1}）- 點：{2}"),
    KO("카메라: {0}(뷰 {1}) - 점: {2}"),
    DE("Kameras: {0} ({1} Ansichten) - Punkte: {2}"),
    FR("Caméras : {0} ({1} vues) - points : {2}"),
    ES("Cámaras: {0} ({1} vistas) - puntos: {2}"),
    PT("Câmeras: {0} ({1} vistas) - pontos: {2}"),
    IT("Fotocamere: {0} ({1} viste) - punti: {2}"),
    NL("Camera's: {0} ({1} weergaven) - punten: {2}"),
    RU("Камеры: {0} (видов: {1}) - точки: {2}"),
    TR("Kamera: {0} ({1} görünüm) - nokta: {2}"));

SS_MSG(change_dataset,
    EN("Change..."),     JA("変更…"),         ZH_HANS("更改…"),    ZH_HANT("變更…"),
    KO("바꾸기…"),        DE("Ändern …"),     FR("Changer…"),     ES("Cambiar…"),
    PT("Alterar…"),      IT("Cambia…"),      NL("Wijzigen…"),    RU("Изменить…"),
    TR("Değiştir…"));

SS_MSG(no_device_found,
    EN("(no device found)"),
    JA("（デバイスが見つかりません）"),
    ZH_HANS("（未找到设备）"),
    ZH_HANT("（找不到裝置）"),
    KO("(장치를 찾지 못함)"),
    DE("(kein Gerät gefunden)"),
    FR("(aucun périphérique trouvé)"),
    ES("(no se encontró ningún dispositivo)"),
    PT("(nenhum dispositivo encontrado)"),
    IT("(nessun dispositivo trovato)"),
    NL("(geen apparaat gevonden)"),
    RU("(устройство не найдено)"),
    TR("(aygıt bulunamadı)"));

SS_MSG(device_unsupported,
    EN(" [unsupported]"),
    JA("（非対応）"),      ZH_HANS("（不支持）"), ZH_HANT("（不支援）"),
    KO(" [지원 안 함]"),  DE(" [nicht unterstützt]"), FR(" [non pris en charge]"),
    ES(" [no compatible]"), PT(" [sem suporte]"), IT(" [non supportato]"),
    NL(" [niet ondersteund]"), RU(" [не поддерживается]"),
    TR(" [desteklenmiyor]"));

SS_MSG(device_auto,
    EN("Auto"),          JA("自動"),          ZH_HANS("自动"),     ZH_HANT("自動"),
    KO("자동"),           DE("Automatisch"),  FR("Automatique"),  ES("Automático"),
    PT("Automático"),    IT("Automatico"),   NL("Automatisch"),  RU("Автоматически"),
    TR("Otomatik"));

SS_MSG(primary_gpu,
    EN("Primary GPU"), JA("メインGPU"), ZH_HANS("主 GPU"), ZH_HANT("主要 GPU"),
    KO("주 GPU"), DE("Primäre GPU"), FR("GPU principal"), ES("GPU principal"),
    PT("GPU principal"), IT("GPU principale"), NL("Primaire GPU"), RU("Основной GPU"),
    TR("Birincil GPU"));

SS_MSG(additional_gpus,
    EN("Additional GPUs"), JA("追加GPU"), ZH_HANS("其他 GPU"), ZH_HANT("其他 GPU"),
    KO("추가 GPU"), DE("Weitere GPUs"), FR("GPU supplémentaires"), ES("GPU adicionales"),
    PT("GPUs adicionais"), IT("GPU aggiuntive"), NL("Extra GPU's"), RU("Дополнительные GPU"),
    TR("Ek GPU'lar"));

SS_MSG(additional_gpus_none,
    EN("None"), JA("なし"), ZH_HANS("无"), ZH_HANT("無"), KO("없음"),
    DE("Keine"), FR("Aucun"), ES("Ninguna"), PT("Nenhuma"), IT("Nessuna"),
    NL("Geen"), RU("Нет"), TR("Yok"));

SS_MSG(additional_gpus_help,
    EN("Extra GPUs share masking and depth & normals with the primary GPU, one process each.\n- Reconstruction and training use only the primary GPU.\n- Auto adds every other GPU of the best kind present: discrete before integrated."),
    JA("追加GPUはメインGPUとマスク、深度と法線の処理を分担し、各GPUで1つのプロセスを使います。再構成と学習はメインGPUのみを使います。自動は独立GPUを優先し、なければ統合GPUを追加します。"),
    ZH_HANS("其他 GPU 与主 GPU 分担遮罩、深度和法线处理，每个 GPU 一个进程。重建和训练仅使用主 GPU。自动添加所有其他最佳类型的 GPU：优先独立 GPU，其次集成 GPU。"),
    ZH_HANT("其他 GPU 與主要 GPU 分擔遮罩、深度和法線處理，每個 GPU 一個程序。重建和訓練僅使用主要 GPU。自動加入所有其他最佳類型的 GPU：優先獨立 GPU，其次整合 GPU。"),
    KO("추가 GPU는 주 GPU와 마스크, 깊이 및 법선 처리를 나누며 GPU마다 프로세스 하나를 사용합니다.\n재구성과 학습은 주 GPU만 사용합니다. 자동은 외장 GPU 우선, 없으면 내장 GPU를 모두 추가합니다."),
    DE("Weitere GPUs teilen Maskierung, Tiefe und Normalen mit der primären GPU, je ein Prozess.\n- Rekonstruktion und Training nutzen nur die primäre GPU.\n- Automatisch ergänzt alle weiteren GPUs der besten vorhandenen Art: dediziert vor integriert."),
    FR("Les GPU supplémentaires partagent masques, profondeur et normales avec le GPU principal, un processus chacun.\n- Reconstruction et entraînement restent sur le GPU principal.\n- Auto ajoute tous les autres GPU du meilleur type présent : dédié avant intégré."),
    ES("Los GPU adicionales comparten máscaras, profundidad y normales con el principal, un proceso por GPU.\n- Reconstrucción y entrenamiento usan solo el principal.\n- Auto añade los demás GPU del mejor tipo disponible: dedicado antes que integrado."),
    PT("As GPUs adicionais partilham máscaras, profundidade e normais com a principal, um processo por GPU.\n- Reconstrução e treino usam só a principal.\n- Auto adiciona todas as outras GPUs do melhor tipo presente: dedicado antes de integrado."),
    IT("Le GPU aggiuntive condividono maschere, profondità e normali con la principale, un processo ciascuna.\n- Ricostruzione e training usano solo la principale.\n- Auto aggiunge le altre GPU del tipo migliore presente: dedicato prima di integrato."),
    NL("Extra GPU's delen maskers, diepte en normalen met de primaire GPU, elk met één proces.\n- Reconstructie en training gebruiken alleen de primaire GPU.\n- Auto voegt alle andere GPU's van het beste aanwezige type toe: afzonderlijk vóór geïntegreerd."),
    RU("Дополнительные GPU делят маски, глубину и нормали с основным GPU, по одному процессу на GPU.\n- Реконструкция и обучение используют только основной GPU.\n- Авто добавляет остальные GPU лучшего доступного типа: дискретные перед встроенными."),
    TR("Ek GPU'lar maskeleme, derinlik ve normalleri birincil GPU ile paylaşır; her birine bir süreç.\n- Yeniden oluşturma ve eğitim yalnızca birincil GPU'yu kullanır.\n- Otomatik, en iyi türdeki diğer tüm GPU'ları ekler: önce ayrık, yoksa tümleşik."));

SS_MSG(device_help,
    EN("GPU for built-in Vulkan jobs; changing it requires a restart.\n- Auto picks the fastest usable GPU.\n- CUDA training and external tools have separate settings."),
    JA("内蔵Vulkan処理用のGPUです。変更には再起動が必要です。\n- 自動は利用可能な最速のGPUを選びます。\n- CUDA学習と外部ツールは別設定です。"),
    ZH_HANS("内置 Vulkan 任务的 GPU；更改后需重启。\n- 自动选择可用的最快 GPU。\n- CUDA 训练和外部工具使用独立设置。"),
    ZH_HANT("內建 Vulkan 任務的 GPU；變更後需重新啟動。\n- 自動選擇可用的最快 GPU。\n- CUDA 訓練與外部工具使用獨立設定。"),
    KO("내장 Vulkan 작업용 GPU입니다. 변경하려면 재시작해야 합니다.\n- 자동은 사용 가능한 가장 빠른 GPU를 선택합니다.\n- CUDA 학습과 외부 도구는 별도 설정을 씁니다."),
    DE("GPU für integrierte Vulkan-Aufgaben; Änderung erfordert Neustart.\n- Auto wählt die schnellste nutzbare GPU.\n- CUDA-Training und externe Tools haben eigene Einstellungen."),
    FR("GPU des tâches Vulkan intégrées ; changement après redémarrage.\n- Auto choisit le GPU utilisable le plus rapide.\n- Réglages distincts pour CUDA et les outils externes."),
    ES("GPU para tareas Vulkan integradas; cambiarla requiere reiniciar.\n- Auto elige la GPU utilizable más rápida.\n- CUDA y las herramientas externas tienen ajustes propios."),
    PT("GPU para tarefas Vulkan integradas; alterar exige reinício.\n- Auto escolhe a GPU utilizável mais rápida.\n- CUDA e ferramentas externas têm definições próprias."),
    IT("GPU per attività Vulkan integrate; la modifica richiede riavvio.\n- Auto sceglie la GPU utilizzabile più veloce.\n- CUDA e gli strumenti esterni hanno impostazioni separate."),
    NL("GPU voor ingebouwde Vulkan-taken; wijzigen vereist herstart.\n- Auto kiest de snelste bruikbare GPU.\n- CUDA-training en externe tools hebben eigen instellingen."),
    RU("GPU для встроенных задач Vulkan; смена требует перезапуска.\n- Авто выбирает самый быстрый доступный GPU.\n- У CUDA и внешних инструментов отдельные настройки."),
    TR("Yerleşik Vulkan işleri için GPU; değişiklik yeniden başlatma gerektirir.\n- Otomatik, kullanılabilir en hızlı GPU’yu seçer.\n- CUDA ve harici araçların ayarları ayrıdır."));

SS_MSG(device_auto_help,
    EN("Picks the best usable GPU: discrete before integrated, then the one with the most memory; "
       "between identical GPUs, the one with more free memory."),
    JA("使用可能なGPUを選びます。独立GPU、統合GPUの順で、同じ種類ではメモリが最大のGPUを、"
       "同じGPU同士では空きメモリが多い方を優先します。"),
    ZH_HANS("选择最佳可用 GPU：优先独立 GPU，其次集成 GPU，同类型中选择内存最多的；"
            "相同的 GPU 之间选择可用内存更多的。"),
    ZH_HANT("選擇最佳可用 GPU：優先獨立 GPU，其次整合 GPU，同類型中選擇記憶體最多的；"
            "相同的 GPU 之間選擇可用記憶體更多的。"),
    KO("사용 가능한 GPU 중 외장, 내장 순으로 선택하고 같은 종류에서는 메모리가 가장 많은 GPU를, "
       "동일한 GPU끼리는 여유 메모리가 더 많은 GPU를 선택합니다."),
    DE("Wählt die beste nutzbare GPU: dediziert vor integriert, dann die mit dem meisten Speicher; "
       "bei baugleichen GPUs die mit mehr freiem Speicher."),
    FR("Choisit le meilleur GPU utilisable : dédié avant intégré, puis celui avec le plus de mémoire ; "
       "entre GPU identiques, celui avec le plus de mémoire libre."),
    ES("Elige el mejor GPU disponible: dedicado antes que integrado, luego el de mayor memoria; "
       "entre GPU idénticos, el de más memoria libre."),
    PT("Escolhe a melhor GPU disponível: dedicada antes de integrada, depois a de maior memória; "
       "entre GPUs idênticas, a de mais memória livre."),
    IT("Sceglie la migliore GPU utilizzabile: dedicata prima di integrata, poi quella con più memoria; "
       "tra GPU identiche, quella con più memoria libera."),
    NL("Kiest de beste bruikbare GPU: afzonderlijk vóór geïntegreerd, daarna die met het meeste geheugen; "
       "bij identieke GPU's die met het meeste vrije geheugen."),
    RU("Выбирает лучший доступный GPU: дискретный перед встроенным, затем с наибольшей памятью; "
       "из одинаковых GPU — с большим объёмом свободной памяти."),
    TR("En iyi kullanılabilir GPU'yu seçer: önce ayrık, sonra tümleşik; ardından en çok belleği olanı; "
       "aynı GPU'lar arasında boş belleği daha çok olanı."));

// {0} device name
SS_MSG(device_frozen_at,
    EN("GPU fixed for this session: {0}"),
    JA("このセッションの GPU: {0}"),
    ZH_HANS("本会话使用的 GPU：{0}"),
    ZH_HANT("本工作階段使用的 GPU：{0}"),
    KO("이 세션의 GPU: {0}"),
    DE("GPU für diese Sitzung festgelegt: {0}"),
    FR("GPU figé pour cette session : {0}"),
    ES("GPU fijada para esta sesión: {0}"),
    PT("GPU fixa para esta sessão: {0}"),
    IT("GPU fissata per questa sessione: {0}"),
    NL("GPU vastgelegd voor deze sessie: {0}"),
    RU("Графический процессор закреплён на этот сеанс: {0}"),
    TR("Bu oturum için GPU sabitlendi: {0}"));

SS_MSG(device_restart_required,
    EN("Changing the native Vulkan GPU needs restarting the application. Built-in Vulkan jobs stay on this device; CUDA training and external tools use separate settings."),
    JA("組み込み Vulkan GPU を変更するにはアプリを再起動してください。組み込み Vulkan 処理はこのデバイスで続き、CUDA 学習と外部ツールは別の設定を使います。"),
    ZH_HANS("更换内置 Vulkan GPU 需要重新启动程序。内置 Vulkan 任务继续使用此设备；CUDA 训练和外部工具使用独立设置。"),
    ZH_HANT("更換內建 Vulkan GPU 需要重新啟動程式。內建 Vulkan 工作會繼續使用此裝置；CUDA 訓練和外部工具使用獨立設定。"),
    KO("내장 Vulkan GPU를 바꾸려면 앱을 다시 시작해야 합니다. 내장 Vulkan 작업은 이 장치를 계속 사용하며, CUDA 학습과 외부 도구는 별도 설정을 사용합니다."),
    DE("Das Ändern der integrierten Vulkan-GPU erfordert einen Neustart der Anwendung.\n- Integrierte Vulkan-Aufträge bleiben auf diesem Gerät;\n- CUDA-Training und externe Werkzeuge verwenden eigene Einstellungen."),
    FR("Changer le GPU Vulkan intégré demande de redémarrer l'application.\n- Les tâches Vulkan intégrées restent sur ce périphérique ;\n- l'entraînement CUDA et les outils externes ont leurs propres réglages."),
    ES("Cambiar la GPU Vulkan integrada requiere reiniciar la aplicación.\n- Las tareas Vulkan integradas siguen en este dispositivo;\n- el entrenamiento CUDA y las herramientas externas usan ajustes separados."),
    PT("Trocar a GPU Vulkan integrada exige reiniciar o aplicativo.\n- As tarefas Vulkan integradas permanecem neste dispositivo;\n- o treinamento CUDA e as ferramentas externas usam configurações separadas."),
    IT("Cambiare la GPU Vulkan integrata richiede di riavviare l'applicazione.\n- Le attività Vulkan integrate restano su questo dispositivo;\n- l'addestramento CUDA e gli strumenti esterni usano impostazioni separate."),
    NL("De ingebouwde Vulkan-GPU wijzigen vereist een herstart van de toepassing.\n- Ingebouwde Vulkan-taken blijven op dit apparaat;\n- CUDA-training en externe hulpmiddelen gebruiken aparte instellingen."),
    RU("Для смены встроенного GPU Vulkan требуется перезапустить программу.\n- Встроенные задачи Vulkan остаются на этом устройстве;\n- обучение CUDA и внешние инструменты используют отдельные настройки."),
    TR("Yerleşik Vulkan GPU'sunu değiştirmek için uygulamayı yeniden başlatmak gerekir. Yerleşik Vulkan işleri bu aygıtta kalır; CUDA eğitimi ve harici araçlar ayrı ayarlar kullanır."));

// {0} the requested value, {1} the frozen one
SS_MSG(device_conflict,
    EN("This session already runs on {1}, so {0} cannot be used. Restart the "
       "application to choose another GPU."),
    JA("このセッションはすでに {1} で動作しているため {0} は使えません。"
       "別の GPU を選ぶにはアプリを再起動してください。"),
    ZH_HANS("本会话已在 {1} 上运行，无法使用 {0}。请重新启动程序以选择其他 GPU。"),
    ZH_HANT("本工作階段已在 {1} 上執行，無法使用 {0}。請重新啟動程式以選擇其他 GPU。"),
    KO("이 세션은 이미 {1} 에서 실행 중이므로 {0} 을(를) 쓸 수 없습니다. 다른 "
       "GPU 를 고르려면 앱을 다시 시작하세요."),
    DE("Diese Sitzung läuft bereits auf {1}, daher kann {0} nicht verwendet "
       "werden. Starten Sie die Anwendung neu, um eine andere GPU zu wählen."),
    FR("Cette session tourne déjà sur {1} ; {0} ne peut donc pas être utilisé. "
       "Redémarrez l'application pour choisir un autre GPU."),
    ES("Esta sesión ya se ejecuta en {1}, así que no se puede usar {0}. "
       "Reinicie la aplicación para elegir otra GPU."),
    PT("Esta sessão já roda em {1}, então {0} não pode ser usado. Reinicie o "
       "aplicativo para escolher outra GPU."),
    IT("Questa sessione gira già su {1}, quindi {0} non può essere usato. "
       "Riavvia l'applicazione per scegliere un'altra GPU."),
    NL("Deze sessie draait al op {1}, dus {0} kan niet worden gebruikt. Herstart "
       "de toepassing om een andere GPU te kiezen."),
    RU("Этот сеанс уже выполняется на {1}, поэтому {0} использовать нельзя. "
       "Перезапустите программу, чтобы выбрать другой GPU."),
    TR("Bu oturum zaten {1} üzerinde çalışıyor, bu yüzden {0} kullanılamaz. "
       "Başka bir GPU seçmek için uygulamayı yeniden başlatın."));

// {0} the requested value, {1} why it was rejected
SS_MSG(device_error,
    EN("Cannot use GPU \"{0}\": {1}"),
    JA("GPU「{0}」は使えません: {1}"),
    ZH_HANS("无法使用 GPU“{0}”：{1}"),
    ZH_HANT("無法使用 GPU「{0}」：{1}"),
    KO("GPU \"{0}\" 을(를) 쓸 수 없습니다: {1}"),
    DE("GPU „{0}“ kann nicht verwendet werden: {1}"),
    FR("Impossible d'utiliser le GPU « {0} » : {1}"),
    ES("No se puede usar la GPU «{0}»: {1}"),
    PT("Não é possível usar a GPU \"{0}\": {1}"),
    IT("Impossibile usare la GPU \"{0}\": {1}"),
    NL("GPU \"{0}\" kan niet worden gebruikt: {1}"),
    RU("Не удаётся использовать GPU «{0}»: {1}"),
    TR("\"{0}\" GPU'su kullanılamıyor: {1}"));

SS_MSG(device_detail_malformed,
    EN("the request is not a valid device selector"),
    JA("要求が有効なデバイス指定ではありません"),
    ZH_HANS("该请求不是有效的设备选择器"),
    ZH_HANT("該要求不是有效的裝置選擇器"),
    KO("요청이 올바른 장치 선택자가 아닙니다"),
    DE("die Angabe ist kein gültiger Geräteauswahlwert"),
    FR("la demande n'est pas un sélecteur de périphérique valide"),
    ES("la solicitud no es un selector de dispositivo válido"),
    PT("a solicitação não é um seletor de dispositivo válido"),
    IT("la richiesta non è un selettore di dispositivo valido"),
    NL("de aanvraag is geen geldige apparaatkiezer"),
    RU("запрос не является допустимым селектором устройства"),
    TR("istek geçerli bir aygıt seçici değil"));

SS_MSG(device_detail_out_of_range,
    EN("no device has this index"),
    JA("この番号のデバイスはありません"),
    ZH_HANS("没有此索引的设备"),
    ZH_HANT("沒有此索引的裝置"),
    KO("이 번호의 장치가 없습니다"),
    DE("kein Gerät hat diesen Index"),
    FR("aucun périphérique n'a cet indice"),
    ES("ningún dispositivo tiene este índice"),
    PT("nenhum dispositivo tem este índice"),
    IT("nessun dispositivo ha questo indice"),
    NL("geen apparaat heeft deze index"),
    RU("устройства с таким индексом нет"),
    TR("bu dizinde bir aygıt yok"));

SS_MSG(device_detail_missing,
    EN("no device matches this request"),
    JA("この指定に一致するデバイスがありません"),
    ZH_HANS("没有与此请求匹配的设备"),
    ZH_HANT("沒有與此要求相符的裝置"),
    KO("이 요청과 일치하는 장치가 없습니다"),
    DE("kein Gerät passt zu dieser Angabe"),
    FR("aucun périphérique ne correspond à cette demande"),
    ES("ningún dispositivo coincide con esta solicitud"),
    PT("nenhum dispositivo corresponde a esta solicitação"),
    IT("nessun dispositivo corrisponde a questa richiesta"),
    NL("geen apparaat komt overeen met deze aanvraag"),
    RU("ни одно устройство не соответствует этому запросу"),
    TR("bu istekle eşleşen aygıt yok"));

SS_MSG(device_detail_no_device,
    EN("no usable Vulkan device was found"),
    JA("使用可能な Vulkan デバイスが見つかりませんでした"),
    ZH_HANS("未找到可用的 Vulkan 设备"),
    ZH_HANT("找不到可用的 Vulkan 裝置"),
    KO("사용 가능한 Vulkan 장치를 찾지 못했습니다"),
    DE("es wurde kein nutzbares Vulkan-Gerät gefunden"),
    FR("aucun périphérique Vulkan utilisable n'a été trouvé"),
    ES("no se encontró ningún dispositivo Vulkan utilizable"),
    PT("nenhum dispositivo Vulkan utilizável foi encontrado"),
    IT("non è stato trovato alcun dispositivo Vulkan utilizzabile"),
    NL("er is geen bruikbaar Vulkan-apparaat gevonden"),
    RU("используемое устройство Vulkan не найдено"),
    TR("kullanılabilir bir Vulkan aygıtı bulunamadı"));

// {0} the environment's selector
SS_MSG(device_inherited_env,
    EN("Inherited from SS_VK_DEVICE: {0}"),
    JA("SS_VK_DEVICE から継承: {0}"),
    ZH_HANS("从 SS_VK_DEVICE 继承：{0}"),
    ZH_HANT("繼承自 SS_VK_DEVICE：{0}"),
    KO("SS_VK_DEVICE 에서 상속: {0}"),
    DE("Von SS_VK_DEVICE übernommen: {0}"),
    FR("Hérité de SS_VK_DEVICE : {0}"),
    ES("Heredado de SS_VK_DEVICE: {0}"),
    PT("Herdado de SS_VK_DEVICE: {0}"),
    IT("Ereditato da SS_VK_DEVICE: {0}"),
    NL("Overgenomen van SS_VK_DEVICE: {0}"),
    RU("Унаследовано из SS_VK_DEVICE: {0}"),
    TR("SS_VK_DEVICE'den devralındı: {0}"));

SS_MSG(device_none_native,
    EN("No usable Vulkan device was found, so nothing native can run."),
    JA("使用可能な Vulkan デバイスが見つからないため、ネイティブ処理は実行"
       "できません。"),
    ZH_HANS("未找到可用的 Vulkan 设备，因此无法运行任何原生任务。"),
    ZH_HANT("找不到可用的 Vulkan 裝置，因此無法執行任何原生工作。"),
    KO("사용 가능한 Vulkan 장치를 찾지 못해 네이티브 작업을 실행할 수 없습니다."),
    DE("Es wurde kein nutzbares Vulkan-Gerät gefunden, daher kann nichts Natives "
       "laufen."),
    FR("Aucun périphérique Vulkan utilisable n'a été trouvé ; rien de natif ne "
       "peut donc tourner."),
    ES("No se encontró ningún dispositivo Vulkan utilizable, así que nada nativo "
       "puede ejecutarse."),
    PT("Nenhum dispositivo Vulkan utilizável foi encontrado, então nada nativo "
       "pode rodar."),
    IT("Non è stato trovato alcun dispositivo Vulkan utilizzabile, quindi nulla "
       "di nativo può girare."),
    NL("Er is geen bruikbaar Vulkan-apparaat gevonden, dus niets natives kan "
       "draaien."),
    RU("Используемого устройства Vulkan не найдено, поэтому встроенные задачи "
       "запустить нельзя."),
    TR("Kullanılabilir bir Vulkan aygıtı bulunamadı, bu yüzden yerel hiçbir şey "
       "çalışamaz."));

SS_MSG(device_unusable_native,
    EN("the device exists but does not meet the requirements of this workflow"),
    JA("デバイスは存在しますが、この処理の必要条件を満たしていません"),
    ZH_HANS("该设备存在，但不满足此任务的要求"),
    ZH_HANT("該裝置存在，但不符合此工作的要求"),
    KO("장치는 있지만 이 작업의 요구 사항을 충족하지 않습니다"),
    DE("Das Gerät ist vorhanden, erfüllt aber nicht die Anforderungen dieses "
       "Ablaufs"),
    FR("Le périphérique existe mais ne répond pas aux exigences de ce flux"),
    ES("El dispositivo existe pero no cumple los requisitos de este flujo"),
    PT("O dispositivo existe mas não atende aos requisitos deste fluxo"),
    IT("Il dispositivo esiste ma non soddisfa i requisiti di questo flusso"),
    NL("Het apparaat bestaat maar voldoet niet aan de eisen van deze "
       "werkstroom"),
    RU("Устройство есть, но оно не отвечает требованиям этого процесса"),
    TR("Aygıt var ama bu iş akışının gereksinimlerini karşılamıyor"));

SS_MSG(device_cuda_locked,
    EN("The CUDA device is fixed at the first engine operation; restart the app "
       "to change it. Native Vulkan work uses its own selection."),
    JA("CUDA デバイスは最初のエンジン処理で固定されます。変更するにはアプリを再起動"
       "してください。ネイティブ Vulkan 処理は独自の選択を使います。"),
    ZH_HANS("CUDA 设备在第一次引擎操作时固定；要更换请重新启动程序。原生 Vulkan "
            "任务使用自己的选择。"),
    ZH_HANT("CUDA 裝置會在第一次引擎操作時固定；要更換請重新啟動程式。原生 Vulkan "
            "工作使用自己的選擇。"),
    KO("CUDA 장치는 첫 엔진 작업에서 고정됩니다. 바꾸려면 앱을 다시 시작하세요. "
       "네이티브 Vulkan 작업은 자체 선택을 사용합니다."),
    DE("Das CUDA-Gerät wird beim ersten Engine-Vorgang festgelegt; zum Wechseln "
       "die Anwendung neu starten. Native Vulkan-Arbeit nutzt eine eigene Auswahl."),
    FR("Le périphérique CUDA est fixé lors de la première opération du moteur ; "
       "redémarrez l'application pour en changer. Le travail Vulkan natif utilise "
       "sa propre sélection."),
    ES("El dispositivo CUDA queda fijado en la primera operación del motor; "
       "reinicie la aplicación para cambiarlo. El trabajo Vulkan nativo usa su "
       "propia selección."),
    PT("O dispositivo CUDA fica fixo na primeira operação do mecanismo; reinicie "
       "o aplicativo para trocá-lo. O trabalho Vulkan nativo usa sua própria seleção."),
    IT("Il dispositivo CUDA viene fissato alla prima operazione del motore; "
       "riavvia l'applicazione per cambiarlo. Il lavoro Vulkan nativo usa una "
       "selezione propria."),
    NL("Het CUDA-apparaat ligt vast bij de eerste enginebewerking; herstart de "
       "toepassing om het te wijzigen. Native Vulkan-werk gebruikt een eigen keuze."),
    RU("Устройство CUDA фиксируется при первой операции движка; чтобы сменить его, "
       "перезапустите программу. Встроенные задачи Vulkan используют собственный "
       "выбор."),
    TR("CUDA aygıtı ilk motor işleminde sabitlenir; değiştirmek için uygulamayı "
       "yeniden başlatın. Yerel Vulkan işi kendi seçimini kullanır."));

// The known-issue banner's link to the issue tracker.
SS_MSG(device_issue_details,
    EN("Details on GitHub"),
    JA("GitHub で詳細を見る"),
    ZH_HANS("在 GitHub 上查看详情"),
    ZH_HANT("在 GitHub 上查看詳情"),
    KO("GitHub에서 자세히 보기"),
    DE("Details auf GitHub"),
    FR("Détails sur GitHub"),
    ES("Detalles en GitHub"),
    PT("Detalhes no GitHub"),
    IT("Dettagli su GitHub"),
    NL("Details op GitHub"),
    RU("Подробности на GitHub"),
    TR("GitHub'da ayrıntılar"));

SS_MSG(link_no_browser,
    EN("Could not open a browser. The page is at {0} (copied to the clipboard)."),
    JA("ブラウザを開けませんでした。ページは {0} にあります"
       "（クリップボードにコピーしました）。"),
    ZH_HANS("无法打开浏览器。页面在 {0}（已复制到剪贴板）。"),
    ZH_HANT("無法開啟瀏覽器。頁面在 {0}（已複製到剪貼簿）。"),
    KO("브라우저를 열지 못했습니다. 페이지는 {0}에 있습니다(클립보드에 "
       "복사했습니다)."),
    DE("Es ließ sich kein Browser öffnen. Die Seite steht unter {0} (in die "
       "Zwischenablage kopiert)."),
    FR("Impossible d'ouvrir un navigateur. La page est à l'adresse {0} "
       "(copiée dans le presse-papiers)."),
    ES("No se pudo abrir un navegador. La página está en {0} (copiada al "
       "portapapeles)."),
    PT("Não foi possível abrir um navegador. A página está em {0} (copiada "
       "para a área de transferência)."),
    IT("Non è stato possibile aprire un browser. La pagina si trova in {0} "
       "(copiata negli appunti)."),
    NL("Er kon geen browser worden geopend. De pagina staat op {0} "
       "(gekopieerd naar het klembord)."),
    RU("Не удалось открыть браузер. Страница находится по адресу {0} "
       "(адрес скопирован в буфер обмена)."),
    TR("Bir tarayıcı açılamadı. Sayfa şu adreste: {0} (panoya kopyalandı)."));

// ---- basic options ----
SS_MSG(opt_output_folder,
    EN("Output folder"), JA("出力フォルダ"),   ZH_HANS("输出文件夹"), ZH_HANT("輸出資料夾"),
    KO("출력 폴더"),      DE("Ausgabeordner"), FR("Dossier de sortie"),
    ES("Carpeta de salida"), PT("Pasta de saída"), IT("Cartella di destinazione"),
    NL("Uitvoermap"),    RU("Папка результатов"), TR("Çıktı klasörü"));

SS_MSG(opt_output_folder_help,
    EN("Where run outputs (checkpoints, splat.ply, config.json) are written. "
       "Each run gets its own subfolder."),
    JA("実行結果（チェックポイント、splat.ply、config.json）の書き出し先です。"
       "実行ごとにサブフォルダが作られます。"),
    ZH_HANS("运行结果（检查点、splat.ply、config.json）的写入位置。"
            "每次运行都会有自己的子文件夹。"),
    ZH_HANT("執行結果（檢查點、splat.ply、config.json）的寫入位置。"
            "每次執行都會有自己的子資料夾。"),
    KO("실행 결과(체크포인트, splat.ply, config.json)를 쓰는 곳입니다. 실행마다 "
       "하위 폴더가 하나씩 생깁니다."),
    DE("Wohin die Ergebnisse eines Laufs (Prüfpunkte, splat.ply, config.json) "
       "geschrieben werden. Jeder Lauf bekommt einen eigenen Unterordner."),
    FR("Où sont écrits les résultats d'une exécution (points de sauvegarde, "
       "splat.ply, config.json). Chaque exécution a son propre sous-dossier."),
    ES("Dónde se escriben los resultados de la ejecución (puntos de control, "
       "splat.ply, config.json). Cada ejecución tiene su propia subcarpeta."),
    PT("Onde os resultados da execução (pontos de verificação, splat.ply, "
       "config.json) são gravados. Cada execução ganha sua própria subpasta."),
    IT("Dove vengono scritti i risultati dell'esecuzione (punti di controllo, "
       "splat.ply, config.json). Ogni esecuzione ha una sua sottocartella."),
    NL("Waar de resultaten van een run (controlepunten, splat.ply, "
       "config.json) worden geschreven. Elke run krijgt een eigen submap."),
    RU("Куда записываются результаты запуска (контрольные точки, splat.ply, "
       "config.json). У каждого запуска своя подпапка."),
    TR("Çalıştırma çıktılarının (denetim noktaları, splat.ply, config.json) "
       "yazıldığı yer. Her çalıştırma kendi alt klasörünü alır."));

SS_MSG(opt_run_name,
    EN("Run name"),      JA("実行名"),        ZH_HANS("运行名称"),  ZH_HANT("執行名稱"),
    KO("실행 이름"),      DE("Laufname"),     FR("Nom de l'exécution"),
    ES("Nombre de la ejecución"), PT("Nome da execução"),
    IT("Nome dell'esecuzione"), NL("Naam van de run"), RU("Имя запуска"),
    TR("Çalıştırma adı"));

SS_MSG(opt_run_name_hint,
    EN("auto: <dataset>_<time>"),
    JA("自動: <データセット>_<時刻>"),
    ZH_HANS("自动：<数据集>_<时间>"),
    ZH_HANT("自動：<資料集>_<時間>"),
    KO("자동: <데이터셋>_<시각>"),
    DE("automatisch: <Datensatz>_<Zeit>"),
    FR("auto : <jeu de données>_<heure>"),
    ES("auto: <conjunto>_<hora>"),
    PT("auto: <conjunto>_<hora>"),
    IT("auto: <set di dati>_<ora>"),
    NL("automatisch: <dataset>_<tijd>"),
    RU("авто: <набор>_<время>"),
    TR("otomatik: <veri kümesi>_<saat>"));

SS_MSG(opt_run_name_help,
    EN("Subfolder name for this run. Leave empty for <dataset>_<timestamp>."),
    JA("この実行のサブフォルダ名です。空欄なら <データセット>_<日時> になります。"),
    ZH_HANS("本次运行的子文件夹名。留空则使用 <数据集>_<时间戳>。"),
    ZH_HANT("本次執行的子資料夾名。留空則使用 <資料集>_<時間戳>。"),
    KO("이번 실행의 하위 폴더 이름입니다. 비워 두면 <데이터셋>_<시각>이 됩니다."),
    DE("Name des Unterordners für diesen Lauf. Leer lassen für "
       "<Datensatz>_<Zeitstempel>."),
    FR("Nom du sous-dossier de cette exécution. Laisser vide pour <jeu de "
       "données>_<horodatage>."),
    ES("Nombre de la subcarpeta de esta ejecución. Déjelo vacío para "
       "<conjunto>_<marca de tiempo>."),
    PT("Nome da subpasta desta execução. Deixe vazio para "
       "<conjunto>_<carimbo de hora>."),
    IT("Nome della sottocartella di questa esecuzione. Lo lasci vuoto per "
       "<set di dati>_<data e ora>."),
    NL("Naam van de submap voor deze run. Laat leeg voor "
       "<dataset>_<tijdstempel>."),
    RU("Имя подпапки для этого запуска. Оставьте пустым, чтобы получить "
       "<набор>_<метка времени>."),
    TR("Bu çalıştırmanın alt klasör adı. Boş bırakılırsa "
       "<veri kümesi>_<zaman damgası> olur."));

SS_MSG(opt_steps,
    EN("Training steps"), JA("学習ステップ数"), ZH_HANS("训练步数"), ZH_HANT("訓練步數"),
    KO("학습 단계 수"),    DE("Trainingsschritte"), FR("Étapes d'entraînement"),
    ES("Pasos de entrenamiento"), PT("Passos de treinamento"),
    IT("Passi di addestramento"), NL("Trainingsstappen"), RU("Шагов обучения"),
    TR("Eğitim adımı"));

SS_MSG(opt_steps_help,
    EN("How long to optimize. 30000 is a solid default; small scenes can look "
       "good at 10000-15000, and large ones sometimes need more than 30000."),
    JA("最適化を続ける長さです。30000 が手堅い既定値で、小さなシーンなら "
       "10000〜15000 でも十分見栄えがします。大きなシーンでは 30000 を超える"
       "こともあります。"),
    ZH_HANS("优化的时长。30000 是稳妥的默认值；小场景在 10000-15000 就已经不错，"
            "大场景有时需要超过 30000。"),
    ZH_HANT("最佳化的時長。30000 是穩妥的預設值；小場景在 10000-15000 就已經不錯，"
            "大場景有時需要超過 30000。"),
    KO("얼마나 오래 최적화할지입니다. 30000이 무난한 기본값이고, 작은 장면은 "
       "10000~15000에서도 보기 좋으며, 큰 장면은 30000을 넘겨야 할 때도 "
       "있습니다."),
    DE("Wie lange optimiert wird. 30000 ist ein solider Standardwert; kleine "
       "Szenen sehen bei 10000-15000 schon gut aus, große brauchen manchmal "
       "mehr als 30000."),
    FR("Durée de l'optimisation. 30000 est une valeur par défaut solide ; les "
       "petites scènes rendent déjà bien à 10000-15000, et les grandes "
       "demandent parfois plus de 30000."),
    ES("Cuánto tiempo optimizar. 30000 es un valor por defecto sólido; las "
       "escenas pequeñas ya se ven bien con 10000-15000, y las grandes a "
       "veces necesitan más de 30000."),
    PT("Por quanto tempo otimizar. 30000 é um padrão sólido; cenas pequenas "
       "já ficam boas com 10000-15000, e as grandes às vezes precisam de mais "
       "de 30000."),
    IT("Per quanto tempo ottimizzare. 30000 è un valore predefinito solido; "
       "le scene piccole rendono bene già a 10000-15000, quelle grandi a "
       "volte chiedono più di 30000."),
    NL("Hoe lang er geoptimaliseerd wordt. 30000 is een degelijke standaard; "
       "kleine scènes zien er bij 10000-15000 al goed uit, grote hebben soms "
       "meer dan 30000 nodig."),
    RU("Сколько длится оптимизация. 30000 — надёжное значение по умолчанию; "
       "небольшие сцены хорошо выглядят уже на 10000-15000, а большим иногда "
       "нужно больше 30000."),
    TR("Ne kadar süre iyileştirileceği. 30000 sağlam bir varsayılandır; küçük "
       "sahneler 10000-15000'de bile iyi görünür, büyükleri bazen 30000'den "
       "fazlasını ister."));

SS_MSG(opt_max_splats,
    EN("Max splats"),    JA("スプラット数の上限"), ZH_HANS("最大泼溅数"),
    ZH_HANT("最大潑濺數"), KO("최대 스플랫 수"),   DE("Maximale Splats"),
    FR("Splats maximum"), ES("Splats máximos"),  PT("Splats máximos"),
    IT("Splat massimi"), NL("Maximaal aantal splats"), RU("Предел числа сплатов"),
    TR("En çok splat"));

SS_MSG(opt_max_splats_help,
    EN("Maximum splat count. More detail costs VRAM and rendering speed; about 1M suits most small scenes."),
    JA("スプラット数の上限です。増やすほど詳細になりますが、VRAM消費と描画時間が増えます。小規模なシーンでは約100万が目安です。"),
    ZH_HANS("高斯数量上限。更多细节会增加显存占用并降低渲染速度；小场景通常约需 100 万。"),
    ZH_HANT("高斯數量上限。更多細節會增加顯存占用並降低算繪速度；小場景通常約需 100 萬。"),
    KO("최대 스플랫 수입니다. 늘리면 세부가 좋아지지만 VRAM 사용량과 렌더링 시간이 증가합니다. 작은 장면은 보통 약 100만 개면 됩니다."),
    DE("Maximale Splat-Anzahl. Mehr Details kosten VRAM und Rendergeschwindigkeit; etwa 1 Mio. reicht für die meisten kleinen Szenen."),
    FR("Nombre maximal de splats. Plus de détails coûtent de la VRAM et ralentissent le rendu ; environ 1 M suffit aux petites scènes."),
    ES("Máximo de splats. Más detalle consume VRAM y ralentiza el renderizado; cerca de 1 M basta para la mayoría de escenas pequeñas."),
    PT("Máximo de splats. Mais detalhe consome VRAM e reduz a velocidade de renderização; cerca de 1 M basta para a maioria das cenas pequenas."),
    IT("Numero massimo di splat. Più dettaglio costa VRAM e rallenta il rendering; circa 1 M basta per la maggior parte delle scene piccole."),
    NL("Maximaal aantal splats. Meer detail kost VRAM en rendersnelheid; circa 1 miljoen volstaat voor de meeste kleine scènes."),
    RU("Максимум сплатов. Больше деталей требует VRAM и замедляет рендеринг; малым сценам обычно хватает около 1 млн."),
    TR("En fazla splat sayısı. Daha fazla ayrıntı VRAM tüketir ve işlemeyi yavaşlatır; çoğu küçük sahneye yaklaşık 1 milyon yeter."));

SS_MSG(opt_primitive,
    EN("Primitive"),     JA("プリミティブ"),   ZH_HANS("基元"),     ZH_HANT("基元"),
    KO("프리미티브"),     DE("Primitiv"),     FR("Primitive"),    ES("Primitiva"),
    PT("Primitiva"),     IT("Primitiva"),    NL("Primitief"),    RU("Примитив"),
    TR("İlkel"));

SS_MSG(opt_primitive_help,
    EN("3dgs: widest viewer support. mip: antialiasing to reduce shimmer. 3dgut: accurate projection for distorted and 360 cameras."),
    JA("3dgs：ビューアとの互換性が最も高い方式。mip：ちらつきを抑えるアンチエイリアス。3dgut：歪みのあるカメラや360度カメラで正確に投影。"),
    ZH_HANS("3dgs：查看器兼容性最广。mip：抗锯齿，减少闪烁。3dgut：为畸变和 360 相机提供准确投影。"),
    ZH_HANT("3dgs：檢視器相容性最廣。mip：反鋸齒，減少閃爍。3dgut：為畸變與 360 相機提供準確投影。"),
    KO("3dgs: 뷰어 호환성 우수. mip: 안티앨리어싱으로 깜박임 감소. 3dgut: 왜곡 및 360 카메라의 정확한 투영."),
    DE("3dgs: breiteste Viewer-Unterstützung. mip: Kantenglättung gegen Flimmern. 3dgut: genaue Projektion für verzeichnete und 360°-Kameras."),
    FR("3dgs : compatibilité maximale. mip : anticrénelage contre le scintillement. 3dgut : projection précise pour caméras avec distorsion ou à 360°."),
    ES("3dgs: máxima compatibilidad. mip: suavizado para reducir parpadeos. 3dgut: proyección precisa para cámaras con distorsión o de 360°."),
    PT("3dgs: maior compatibilidade. mip: antisserrilhamento para reduzir cintilação. 3dgut: projeção precisa para câmaras com distorção ou de 360°."),
    IT("3dgs: massima compatibilità. mip: antialiasing contro lo sfarfallio. 3dgut: proiezione accurata per camere con distorsione o a 360°."),
    NL("3dgs: breedste viewerondersteuning. mip: antialiasing tegen flikkering. 3dgut: nauwkeurige projectie voor vervormde en 360°-camera’s."),
    RU("3dgs: лучшая совместимость. mip: сглаживание против мерцания. 3dgut: точная проекция для камер с дисторсией и камер 360°."),
    TR("3dgs: en geniş görüntüleyici desteği. mip: titreşimi azaltan kenar yumuşatma. 3dgut: bozulmalı ve 360° kameralar için doğru izdüşüm."));

SS_MSG(opt_primitive_3dgut_warn,
    EN("3dgut generally trains slower than 3dgs, and few existing viewers can "
       "open what it produces."),
    JA("3dgut は 3dgs より学習が遅く、その結果を開けるビューアも限られます。"),
    ZH_HANS("3dgut 的训练通常比 3dgs 慢，能打开它的结果的现有查看器也很少。"),
    ZH_HANT("3dgut 的訓練通常比 3dgs 慢，能開啟它的結果的現有檢視器也很少。"),
    KO("3dgut은 3dgs보다 학습이 대체로 느리고, 그 결과를 열 수 있는 기존 "
       "뷰어도 적습니다."),
    DE("3dgut trainiert meist langsamer als 3dgs, und nur wenige vorhandene "
       "Betrachter können das Ergebnis öffnen."),
    FR("3dgut s'entraîne généralement plus lentement que 3dgs, et peu de "
       "visionneuses existantes savent ouvrir ce qu'il produit."),
    ES("3dgut suele entrenar más lento que 3dgs y pocos visores existentes "
       "abren lo que produce."),
    PT("3dgut costuma treinar mais devagar que 3dgs e poucos visualizadores "
       "existentes abrem o que ele produz."),
    IT("3dgut di solito si addestra più lentamente di 3dgs e pochi "
       "visualizzatori esistenti aprono ciò che produce."),
    NL("3dgut traint doorgaans langzamer dan 3dgs en weinig bestaande viewers "
       "kunnen openen wat het oplevert."),
    RU("3dgut обычно обучается медленнее, чем 3dgs, и мало какие существующие "
       "просмотрщики открывают его результат."),
    TR("3dgut genellikle 3dgs'den daha yavaş eğitilir ve ürettiğini açabilen "
       "görüntüleyici azdır."));

SS_MSG(opt_resolution,
    EN("Image resolution"), JA("画像の解像度"), ZH_HANS("图像分辨率"),
    ZH_HANT("影像解析度"),  KO("이미지 해상도"), DE("Bildauflösung"),
    FR("Résolution d'image"), ES("Resolución de imagen"),
    PT("Resolução da imagem"), IT("Risoluzione immagine"),
    NL("Beeldresolutie"), RU("Разрешение изображений"), TR("Görüntü çözünürlüğü"));

SS_MSG(opt_resolution_native,
    EN("native"),        JA("元のまま"),      ZH_HANS("原始"),     ZH_HANT("原始"),
    KO("원본"),           DE("original"),     FR("d'origine"),    ES("original"),
    PT("original"),      IT("originale"),    NL("origineel"),    RU("исходное"),
    TR("özgün"));

SS_MSG(opt_resolution_help,
    EN("Train at a fraction of the input resolution. Downscaling trains much faster and saves VRAM; use it for 4K+ footage or quick previews."),
    JA("入力解像度を落として学習します。縮小すると学習はずっと速くなり VRAM も節約できます。4K 以上の素材や、ざっと確認したいときに向きます。"),
    ZH_HANS("按输入分辨率的一部分来训练。缩小后训练快得多，也更省显存；适合 4K 以上素材或快速预览。"),
    ZH_HANT("按輸入解析度的一部分來訓練。縮小後訓練快得多，也更省顯示記憶體；適合 4K 以上素材或快速預覽。"),
    KO("입력 해상도의 일부만 써서 학습합니다. 축소하면 학습이 훨씬 빠르고 VRAM도 아낍니다. 4K 이상 소재나 빠른 미리보기에 알맞습니다."),
    DE("Mit einem Bruchteil der Eingangsauflösung trainieren. Herunterskalieren trainiert deutlich schneller und spart VRAM; sinnvoll für 4K-Material und schnelle Vorschauen."),
    FR("Entraîner à une fraction de la résolution d'entrée.\n- Réduire accélère beaucoup l'entraînement et économise la VRAM ;\n- utile pour des rushes en 4K et plus, ou pour un aperçu rapide."),
    ES("Entrenar a una fracción de la resolución de entrada. Reducir entrena mucho más rápido y ahorra VRAM; útil para material 4K o más, y para vistas previas rápidas."),
    PT("Treinar com uma fração da resolução de entrada. Reduzir treina muito mais rápido e economiza VRAM; útil para material 4K ou maior e para prévias rápidas."),
    IT("Addestrare a una frazione della risoluzione d'ingresso. Ridurre addestra molto più in fretta e risparmia VRAM; utile per materiale 4K o superiore e per anteprime rapide."),
    NL("Trainen op een fractie van de invoerresolutie. Verkleinen traint veel sneller en bespaart VRAM; handig bij 4K-materiaal of snelle voorbeelden."),
    RU("Обучать на доле исходного разрешения. Уменьшение сильно ускоряет обучение и экономит видеопамять; пригодится для материала 4K и выше или для быстрого просмотра."),
    TR("Girdi çözünürlüğünün bir kesrinde eğitin. Küçültmek eğitimi çok hızlandırır ve VRAM'den tasarruf ettirir; 4K ve üstü çekimler ya da hızlı önizleme için uygundur."));

SS_MSG(opt_mask_mode,
    EN("Mask mode"),     JA("マスクの扱い"),   ZH_HANS("蒙版模式"),  ZH_HANT("遮罩模式"),
    KO("마스크 모드"),    DE("Maskenmodus"),  FR("Mode de masque"),
    ES("Modo de máscara"), PT("Modo de máscara"), IT("Modalità maschera"),
    NL("Maskermodus"),   RU("Режим маски"),  TR("Maske kipi"));

SS_MSG(opt_mask_mode_exclude,
    EN("Ignore distractors"),
    JA("邪魔物を無視"),   ZH_HANS("忽略干扰物"), ZH_HANT("忽略干擾物"),
    KO("방해물 무시"),    DE("Störendes ignorieren"),
    FR("Ignorer les gêneurs"), ES("Ignorar los elementos molestos"),
    PT("Ignorar o que atrapalha"), IT("Ignorare i disturbi"),
    NL("Storende dingen negeren"), RU("Игнорировать помехи"),
    TR("Rahatsız edicileri yok say"));

SS_MSG(opt_mask_mode_cut_out,
    EN("Cut out background"),
    JA("背景を切り抜く"),  ZH_HANS("裁掉背景"),  ZH_HANT("裁掉背景"),
    KO("배경 잘라내기"),   DE("Hintergrund freistellen"),
    FR("Détourer l'arrière-plan"), ES("Recortar el fondo"),
    PT("Recortar o fundo"), IT("Ritagliare lo sfondo"),
    NL("Achtergrond uitsnijden"), RU("Вырезать фон"),
    TR("Arka planı ayır"));

SS_MSG(opt_mask_mode_off,
    EN("Don't use masks"),
    JA("マスクを使わない"), ZH_HANS("不使用蒙版"), ZH_HANT("不使用遮罩"),
    KO("마스크 사용 안 함"), DE("Masken nicht verwenden"),
    FR("Ne pas utiliser de masques"), ES("No usar máscaras"),
    PT("Não usar máscaras"), IT("Non usare le maschere"),
    NL("Maskers niet gebruiken"), RU("Не использовать маски"),
    TR("Maskeleri kullanma"));

SS_MSG(opt_mask_mode_help,
    EN("Transparent pixels count as masked. Cut out is the default when only alpha masks are present.\n- Ignore distractors — Skip masked pixels in the loss.\n- Cut out background — Train masked pixels as empty."),
    JA("透明画素はマスク対象です。アルファマスクだけなら既定で背景を切り抜きます。\n- 邪魔物を無視 — マスク画素を損失から除外。\n- 背景を切り抜く — マスク画素を空として学習。"),
    ZH_HANS("透明像素视为已蒙版。仅有透明通道蒙版时，默认裁掉背景。\n- 忽略干扰物 — 损失忽略蒙版像素。\n- 裁掉背景 — 将蒙版像素训练为空。"),
    ZH_HANT("透明像素視為已遮罩。僅有透明通道遮罩時，預設裁掉背景。\n- 忽略干擾物 — 損失忽略遮罩像素。\n- 裁掉背景 — 將遮罩像素訓練為空。"),
    KO("투명 픽셀은 마스크로 처리합니다. 알파 마스크만 있으면 기본으로 배경을 잘라냅니다.\n- 방해물 무시 — 마스크 픽셀을 손실에서 제외합니다.\n- 배경 잘라내기 — 마스크 픽셀을 빈 공간으로 학습합니다."),
    DE("Transparente Pixel gelten als maskiert. Bei reinen Alpha-Masken wird der Hintergrund standardmäßig freigestellt.\n- Störendes ignorieren — Maskierte Pixel im Loss auslassen.\n- Hintergrund freistellen — Maskierte Pixel als leer trainieren."),
    FR("Les pixels transparents sont masqués. Le détourage est le réglage par défaut avec les seuls masques alpha.\n- Ignorer les gêneurs — Exclut les pixels masqués de la perte.\n- Détourer l'arrière-plan — Entraîne les pixels masqués comme vides."),
    ES("Los píxeles transparentes cuentan como máscara. Con solo máscaras alfa, se recorta el fondo por defecto.\n- Ignorar los elementos molestos — Excluye píxeles enmascarados de la pérdida.\n- Recortar el fondo — Entrena los píxeles enmascarados como vacíos."),
    PT("Píxeis transparentes contam como mascarados. Com apenas máscaras alfa, o fundo é recortado por padrão.\n- Ignorar o que atrapalha — Exclui píxeis mascarados da perda.\n- Recortar o fundo — Treina píxeis mascarados como vazios."),
    IT("I pixel trasparenti contano come mascherati. Con sole maschere alfa, si ritaglia lo sfondo per impostazione predefinita.\n- Ignorare i disturbi — Esclude i pixel mascherati dalla perdita.\n- Ritagliare lo sfondo — Addestra i pixel mascherati come vuoti."),
    NL("Transparante pixels tellen als gemaskeerd. Bij uitsluitend alfamaskers wordt de achtergrond standaard uitgesneden.\n- Storende dingen negeren — Slaat gemaskeerde pixels over in het verlies.\n- Achtergrond uitsnijden — Traint gemaskeerde pixels als leeg."),
    RU("Прозрачные пиксели считаются замаскированными. Если маски только из альфа-канала, фон вырезается по умолчанию.\n- Игнорировать помехи — Исключает пиксели маски из потерь.\n- Вырезать фон — Обучает пиксели маски как пустые."),
    TR("Saydam pikseller maskeli sayılır. Yalnız alfa maskeleri varsa arka plan varsayılan olarak ayrılır.\n- Rahatsız edicileri yok say — Maskeli pikselleri kayıptan çıkarır.\n- Arka planı ayır — Maskeli pikselleri boş olarak eğitir."));

SS_MSG(opt_sh_degree,
    EN("Color detail (SH)"),
    JA("色の細かさ（SH）"),
    ZH_HANS("颜色细节（SH）"),
    ZH_HANT("顏色細節（SH）"),
    KO("색 디테일(SH)"),
    DE("Farbdetail (SH)"),
    FR("Détail des couleurs (SH)"),
    ES("Detalle de color (SH)"),
    PT("Detalhe de cor (SH)"),
    IT("Dettaglio del colore (SH)"),
    NL("Kleurdetail (SH)"),
    RU("Детальность цвета (SH)"),
    TR("Renk ayrıntısı (SH)"));

SS_MSG(opt_sh_degree_help,
    EN("View-dependent color detail. 3: standard; 0: flat color, smallest model; 4: limited viewer support."),
    JA("視点依存の色の詳細度です。3：標準、0：均一な色で最小のモデル、4：対応ビューアが限られます。"),
    ZH_HANS("视角相关颜色的细节等级。3：标准；0：纯色、模型最小；4：查看器支持有限。"),
    ZH_HANT("視角相關顏色的細節等級。3：標準；0：純色、模型最小；4：檢視器支援有限。"),
    KO("시점에 따른 색상 세부 수준입니다. 3: 표준, 0: 단색과 최소 모델, 4: 뷰어 지원 제한."),
    DE("Blickabhängige Farbdetails. 3: Standard; 0: flache Farben, kleinstes Modell; 4: eingeschränkte Viewer-Unterstützung."),
    FR("Détail des couleurs selon la vue. 3 : standard ; 0 : couleurs plates, modèle minimal ; 4 : compatibilité limitée."),
    ES("Detalle del color según la vista. 3: estándar; 0: color plano, modelo mínimo; 4: compatibilidad limitada."),
    PT("Detalhe da cor conforme a vista. 3: padrão; 0: cor uniforme, modelo mínimo; 4: suporte limitado em visualizadores."),
    IT("Dettaglio del colore secondo la vista. 3: standard; 0: colore piatto, modello minimo; 4: supporto limitato nei visualizzatori."),
    NL("Kleurdetail afhankelijk van de kijkrichting. 3: standaard; 0: vlakke kleur, kleinste model; 4: beperkte viewerondersteuning."),
    RU("Детализация цвета в зависимости от вида. 3: стандарт; 0: плоский цвет, минимальная модель; 4: ограниченная поддержка просмотрщиков."),
    TR("Görüşe bağlı renk ayrıntısı. 3: standart; 0: düz renk, en küçük model; 4: sınırlı görüntüleyici desteği."));

SS_MSG(opt_dataset_depths,
    EN("Use dataset depth maps"), JA("データセットの深度マップを使う"), ZH_HANS("使用数据集深度图"),
    ZH_HANT("使用資料集深度圖"), KO("데이터셋 깊이 맵 사용"), DE("Tiefenkarten des Datensatzes verwenden"),
    FR("Utiliser les cartes de profondeur du jeu de données"),
    ES("Usar los mapas de profundidad del conjunto de datos"),
    PT("Usar os mapas de profundidade do conjunto de dados"), IT("Usa le mappe di profondità del set di dati"),
    NL("Dieptekaarten van de dataset gebruiken"), RU("Использовать карты глубины набора данных"),
    TR("Veri kümesinin derinlik haritalarını kullan"));
SS_MSG(opt_dataset_depths_help,
    EN("Let the depth maps in this dataset guide the geometry. Helps plain, textureless areas; a "
       "badly wrong map can pull quality down."),
    JA("データセットの深度マップで形状を導きます。模様の少ない領域に効きますが、大きく誤った"
       "マップは品質を下げることがあります。"),
    ZH_HANS("用数据集中的深度图引导几何。对无纹理区域有帮助；明显错误的深度图可能降低质量。"),
    ZH_HANT("用資料集中的深度圖引導幾何。對無紋理區域有幫助；明顯錯誤的深度圖可能降低品質。"),
    KO("데이터셋의 깊이 맵으로 형상을 안내합니다. 무늬 없는 영역에 도움이 되지만, 크게 틀린 맵은 "
       "품질을 떨어뜨릴 수 있습니다."),
    DE("Die Tiefenkarten des Datensatzes führen die Geometrie. Hilft in texturlosen Bereichen; eine "
       "stark falsche Karte kann die Qualität senken."),
    FR("Les cartes de profondeur du jeu de données guident la géométrie. Utile sur les zones sans "
       "texture ; une carte très fausse peut dégrader la qualité."),
    ES("Los mapas de profundidad del conjunto de datos guían la geometría. Ayuda en zonas sin textura; "
       "un mapa muy erróneo puede empeorar la calidad."),
    PT("Os mapas de profundidade do conjunto de dados guiam a geometria. Ajuda em áreas sem textura; "
       "um mapa muito errado pode piorar a qualidade."),
    IT("Le mappe di profondità del set di dati guidano la geometria. Aiuta nelle zone senza texture; "
       "una mappa molto errata può peggiorare la qualità."),
    NL("De dieptekaarten van de dataset sturen de geometrie. Helpt bij textuurloze vlakken; een sterk "
       "foute kaart kan de kwaliteit verlagen."),
    RU("Карты глубины набора данных направляют геометрию. Помогает на участках без текстуры; сильно "
       "ошибочная карта может ухудшить качество."),
    TR("Veri kümesindeki derinlik haritaları geometriye yön verir. Dokusuz alanlarda yardımcı olur; çok "
       "hatalı bir harita kaliteyi düşürebilir."));
SS_MSG(opt_dataset_normals,
    EN("Use dataset normal maps"), JA("データセットの法線マップを使う"), ZH_HANS("使用数据集法线图"),
    ZH_HANT("使用資料集法線圖"), KO("데이터셋 노멀 맵 사용"), DE("Normalenkarten des Datensatzes verwenden"),
    FR("Utiliser les cartes de normales du jeu de données"),
    ES("Usar los mapas de normales del conjunto de datos"),
    PT("Usar os mapas de normais do conjunto de dados"), IT("Usa le mappe delle normali del set di dati"),
    NL("Normaalkaarten van de dataset gebruiken"), RU("Использовать карты нормалей набора данных"),
    TR("Veri kümesinin normal haritalarını kullan"));
SS_MSG(opt_dataset_normals_help,
    EN("Let the normal maps in this dataset guide which way surfaces face. Helps flat surfaces come "
       "out flat."),
    JA("データセットの法線マップで面の向きを導きます。平らな面が平らに仕上がりやすくなります。"),
    ZH_HANS("用数据集中的法线图引导表面朝向，让平面更平整。"),
    ZH_HANT("用資料集中的法線圖引導表面朝向，讓平面更平整。"),
    KO("데이터셋의 노멀 맵으로 표면 방향을 안내합니다. 평평한 면이 평평하게 나오도록 돕습니다."),
    DE("Die Normalenkarten des Datensatzes geben die Ausrichtung der Flächen vor. Ebene Flächen "
       "werden so eben."),
    FR("Les cartes de normales du jeu de données guident l'orientation des surfaces. Les surfaces "
       "planes restent planes."),
    ES("Los mapas de normales del conjunto de datos guían la orientación de las superficies. Ayuda a "
       "que las superficies planas salgan planas."),
    PT("Os mapas de normais do conjunto de dados guiam a orientação das superfícies. Ajuda as "
       "superfícies planas a saírem planas."),
    IT("Le mappe delle normali del set di dati guidano l'orientamento delle superfici. Aiuta le "
       "superfici piane a restare piane."),
    NL("De normaalkaarten van de dataset sturen de richting van oppervlakken. Vlakke oppervlakken "
       "worden zo vlak."),
    RU("Карты нормалей набора данных задают ориентацию поверхностей. Плоские поверхности получаются "
       "плоскими."),
    TR("Veri kümesindeki normal haritaları yüzeylerin yönüne rehberlik eder. Düz yüzeylerin düz "
       "çıkmasına yardımcı olur."));
SS_MSG(opt_bilateral_grid,
    EN("Bilateral Grid color correction"),
    JA("バイラテラルグリッドによる色補正"),
    ZH_HANS("双边网格颜色校正"),
    ZH_HANT("雙邊網格色彩校正"),
    KO("양방향 그리드 색 보정"),
    DE("Farbkorrektur per Bilateral Grid"),
    FR("Correction des couleurs par grille bilatérale"),
    ES("Corrección de color con rejilla bilateral"),
    PT("Correção de cor com grade bilateral"),
    IT("Correzione del colore con griglia bilaterale"),
    NL("Kleurcorrectie met bilateraal raster"),
    RU("Цветокоррекция билатеральной сеткой"),
    TR("Çift yönlü ızgarayla renk düzeltme"));

SS_MSG(opt_bilateral_grid_help,
    EN("Use a bilateral grid to correct color variation across images. Suitable for changing environment lighting. Uncheck for faster and more memory efficient training."),
    JA("画像ごとの色のばらつきをバイラテラルグリッドで補正します。環境光が変わる撮影に向きます。外すと学習は速く、メモリ効率もよくなります。"),
    ZH_HANS("用双边网格校正各张图像之间的颜色差异。适合环境光会变化的拍摄。取消勾选可让训练更快、更省内存。"),
    ZH_HANT("用雙邊網格校正各張影像之間的顏色差異。適合環境光會變化的拍攝。取消勾選可讓訓練更快、更省記憶體。"),
    KO("양방향 그리드로 이미지 간 색 편차를 보정합니다. 주변광이 변하는 촬영에 알맞습니다. 체크를 해제하면 학습이 더 빠르고 메모리도 덜 씁니다."),
    DE("Farbschwankungen zwischen den Bildern mit einem Bilateral Grid ausgleichen. Passend bei wechselndem Umgebungslicht. Abgeschaltet trainiert es schneller und speichersparender."),
    FR("Corriger les écarts de couleur entre les images avec une grille bilatérale.\n- Adapté à un éclairage ambiant qui change.\n- Décoché, l'entraînement est plus rapide et consomme moins de mémoire."),
    ES("Corregir la variación de color entre imágenes con una rejilla bilateral. Adecuado cuando cambia la luz ambiente.\nSin marcar, el entrenamiento es más rápido y usa menos memoria."),
    PT("Corrigir a variação de cor entre as imagens com uma grade bilateral. Adequado quando a luz do ambiente muda.\nDesmarcado, o treinamento fica mais rápido e usa menos memória."),
    IT("Correggere la variazione di colore tra le immagini con una griglia bilaterale.\nAdatto quando la luce ambientale cambia. Deselezionato, l'addestramento è più rapido e usa meno memoria."),
    NL("Kleurverschillen tussen de beelden corrigeren met een bilateraal raster. Geschikt bij wisselend omgevingslicht. Uitgevinkt traint sneller en zuiniger met geheugen."),
    RU("Выравнивать различия цвета между снимками билатеральной сеткой. Подходит, когда меняется освещение. Без флажка обучение быстрее и экономнее по памяти."),
    TR("Görüntüler arasındaki renk farkını çift yönlü ızgarayla düzeltir. Ortam ışığının değiştiği çekimler için uygundur.\nİşareti kaldırmak eğitimi hızlandırır ve belleği daha az kullanır."));

SS_MSG(opt_ppisp,
    EN("PPISP color correction"),
    JA("PPISP による色補正"),
    ZH_HANS("PPISP 颜色校正"),
    ZH_HANT("PPISP 色彩校正"),
    KO("PPISP 색 보정"),
    DE("PPISP-Farbkorrektur"),
    FR("Correction des couleurs PPISP"),
    ES("Corrección de color PPISP"),
    PT("Correção de cor PPISP"),
    IT("Correzione del colore PPISP"),
    NL("PPISP-kleurcorrectie"),
    RU("Цветокоррекция PPISP"),
    TR("PPISP renk düzeltme"));

SS_MSG(opt_ppisp_help,
    EN("Use PPISP to correct color variation across images. Suitable for camera vignetting and exposure/white-balance changes. Uncheck for faster training."),
    JA("画像ごとの色のばらつきを PPISP で補正します。レンズの周辺減光や、露出・ホワイトバランスの変化に向きます。外すと学習が速くなります。"),
    ZH_HANS("用 PPISP 校正各张图像之间的颜色差异。适合镜头暗角以及曝光／白平衡的变化。取消勾选可让训练更快。"),
    ZH_HANT("用 PPISP 校正各張影像之間的顏色差異。適合鏡頭暗角以及曝光／白平衡的變化。取消勾選可讓訓練更快。"),
    KO("PPISP로 이미지 간 색 편차를 보정합니다. 렌즈 비네팅과 노출·화이트밸런스 변화에 알맞습니다. 체크를 해제하면 학습이 더 빨라집니다."),
    DE("Farbschwankungen zwischen den Bildern mit PPISP ausgleichen. Passend bei Vignettierung und wechselnder Belichtung oder Weißabgleich. Abgeschaltet trainiert es schneller."),
    FR("Corriger les écarts de couleur entre les images avec PPISP. Adapté au vignetage et aux changements d'exposition ou de balance des blancs. Décoché, l'entraînement est plus rapide."),
    ES("Corregir la variación de color entre imágenes con PPISP.\n- Adecuado para el viñeteo y los cambios de exposición o balance de blancos.\n- Sin marcar, el entrenamiento es más rápido."),
    PT("Corrigir a variação de cor entre as imagens com PPISP. Adequado para vinhetagem e mudanças de exposição ou balanço de branco. Desmarcado, o treinamento fica mais rápido."),
    IT("Correggere la variazione di colore tra le immagini con PPISP.\n- Adatto alla vignettatura e ai cambi di esposizione o bilanciamento del bianco.\n- Deselezionato, l'addestramento è più rapido."),
    NL("Kleurverschillen tussen de beelden corrigeren met PPISP. Geschikt bij vignettering en wisselende belichting of witbalans. Uitgevinkt traint sneller."),
    RU("Выравнивать различия цвета между снимками с помощью PPISP. Подходит при виньетировании и изменениях экспозиции или баланса белого. Без флажка обучение быстрее."),
    TR("Görüntüler arasındaki renk farkını PPISP ile düzeltir. Vinyetleme ve pozlama / beyaz dengesi değişimleri için uygundur. İşareti kaldırmak eğitimi hızlandırır."));

SS_MSG(opt_distraction_warn,
    EN("Enable only for unmasked moving objects. On a clean capture, this reduces detail without benefit."),
    JA("マスクされていない動く対象がある場合だけ有効にしてください。動くものがない撮影では詳細が失われ、利点がありません。"),
    ZH_HANS("仅在有未遮罩的移动对象时启用。干净的拍摄会白白损失细节。"),
    ZH_HANT("僅在有未遮罩的移動對象時啟用。乾淨的拍攝會白白損失細節。"),
    KO("마스크되지 않은 움직이는 대상이 있을 때만 켜세요. 깨끗한 촬영에서는 이점 없이 세부만 줄어듭니다."),
    DE("Nur bei unmaskierten bewegten Objekten aktivieren. Bei sauberen Aufnahmen gehen ohne Nutzen Details verloren."),
    FR("Activer seulement pour des objets mobiles non masqués. Sinon, cela réduit les détails sans bénéfice."),
    ES("Activa solo si hay objetos móviles sin máscara. En capturas limpias reduce el detalle sin aportar mejoras."),
    PT("Ative só para objetos em movimento sem máscara. Em capturas limpas reduz o detalhe sem benefício."),
    IT("Attiva solo per oggetti in movimento senza maschera. Su riprese pulite riduce i dettagli senza vantaggi."),
    NL("Alleen inschakelen bij ongemaskeerde bewegende objecten. Bij schone opnamen gaat detail verloren zonder voordeel."),
    RU("Включайте только для движущихся объектов без маски. На чистой съёмке это лишь снижает детализацию."),
    TR("Yalnız maskelenmemiş hareketli nesneler için açın. Temiz çekimde yarar sağlamadan ayrıntıyı azaltır."));

// ---- controls ----
SS_MSG(training_complete,
    EN("Training complete."),
    JA("学習が完了しました。"),
    ZH_HANS("训练完成。"),
    ZH_HANT("訓練完成。"),
    KO("학습이 끝났습니다."),
    DE("Training abgeschlossen."),
    FR("Entraînement terminé."),
    ES("Entrenamiento terminado."),
    PT("Treinamento concluído."),
    IT("Addestramento completato."),
    NL("Training voltooid."),
    RU("Обучение завершено."),
    TR("Eğitim tamamlandı."));

SS_MSG(training_stopped_unsaved,
    EN("Stopped without saving."),
    JA("保存せずに停止しました。"),
    ZH_HANS("已停止，未保存。"),
    ZH_HANT("已停止，未儲存。"),
    KO("저장하지 않고 멈췄습니다."),
    DE("Ohne Speichern angehalten."),
    FR("Arrêté sans enregistrer."),
    ES("Detenido sin guardar."),
    PT("Parado sem salvar."),
    IT("Fermato senza salvare."),
    NL("Gestopt zonder opslaan."),
    RU("Остановлено без сохранения."),
    TR("Kaydetmeden durduruldu."));

SS_MSG(unsaved_note,
    EN("Nothing more was written; what is in {0} is the last automatic save."),
    JA("以降は何も書き出していません。{0} にあるのは直前の自動保存までです。"),
    ZH_HANS("之后没有再写入任何东西；{0} 里的是上一次自动保存的内容。"),
    ZH_HANT("之後沒有再寫入任何東西；{0} 裡的是上一次自動儲存的內容。"),
    KO("그 뒤로는 아무것도 쓰지 않았습니다. {0}에 있는 것은 직전 자동 저장까지입니다."),
    DE("Es wurde nichts mehr geschrieben; in {0} steht die letzte automatische "
       "Speicherung."),
    FR("Rien de plus n'a été écrit ; ce qui est dans {0} est la dernière "
       "sauvegarde automatique."),
    ES("No se escribió nada más; lo que hay en {0} es el último guardado "
       "automático."),
    PT("Nada mais foi escrito; o que está em {0} é o último salvamento "
       "automático."),
    IT("Non è stato scritto altro; quello che sta in {0} è l'ultimo "
       "salvataggio automatico."),
    NL("Er is niets meer weggeschreven; wat in {0} staat is de laatste "
       "automatische opslag."),
    RU("Больше ничего не записано; в {0} лежит последнее автоматическое "
       "сохранение."),
    TR("Başka bir şey yazılmadı; {0} içindeki son otomatik kayıttır."));

SS_MSG(saved_to,
    EN("Saved to {0}"),  JA("{0} に保存しました"), ZH_HANS("已保存到 {0}"),
    ZH_HANT("已儲存到 {0}"), KO("{0}에 저장했습니다"), DE("Gespeichert unter {0}"),
    FR("Enregistré dans {0}"), ES("Guardado en {0}"), PT("Salvo em {0}"),
    IT("Salvato in {0}"), NL("Opgeslagen in {0}"), RU("Сохранено в {0}"),
    TR("{0} konumuna kaydedildi"));

SS_MSG(start_training,
    EN("Start Training"), JA("学習を開始"),     ZH_HANS("开始训练"),  ZH_HANT("開始訓練"),
    KO("학습 시작"),      DE("Training starten"), FR("Lancer l'entraînement"),
    ES("Iniciar el entrenamiento"), PT("Iniciar o treinamento"),
    IT("Avvia l'addestramento"), NL("Training starten"), RU("Начать обучение"),
    TR("Eğitimi başlat"));

SS_MSG(train_again,
    EN("Train Again"),   JA("もう一度学習"),   ZH_HANS("再次训练"),  ZH_HANT("再次訓練"),
    KO("다시 학습"),      DE("Erneut trainieren"), FR("Réentraîner"),
    ES("Entrenar de nuevo"), PT("Treinar de novo"), IT("Addestra di nuovo"),
    NL("Opnieuw trainen"), RU("Обучить снова"), TR("Yeniden eğit"));

SS_MSG(preparing_engine,
    EN("Preparing engine (seeding splats, caching images) ..."),
    JA("エンジンを準備しています（スプラットの初期化、画像のキャッシュ）…"),
    ZH_HANS("正在准备引擎（生成初始泼溅、缓存图像）…"),
    ZH_HANT("正在準備引擎（產生初始潑濺、快取影像）…"),
    KO("엔진을 준비하는 중(스플랫 초기화, 이미지 캐시)…"),
    DE("Engine wird vorbereitet (Splats werden gesät, Bilder gepuffert) …"),
    FR("Préparation du moteur (amorçage des splats, mise en cache des "
       "images)…"),
    ES("Preparando el motor (sembrando splats, cacheando imágenes)…"),
    PT("Preparando o motor (semeando splats, armazenando imagens em cache)…"),
    IT("Preparazione del motore (semina degli splat, cache delle immagini)…"),
    NL("Engine wordt voorbereid (splats zaaien, beelden cachen)…"),
    RU("Подготовка движка (создание начальных сплатов, кэширование "
       "изображений)…"),
    TR("Motor hazırlanıyor (splat'lar tohumlanıyor, görüntüler önbelleğe "
       "alınıyor)…"));

// {0} a file name, {1} FileDownload's status ("42% of 553 MB").
SS_MSG(fetching_lpips,
    EN("Downloading the LPIPS checkpoint {0}: {1}"),
    JA("LPIPS チェックポイント {0} をダウンロード中: {1}"),
    ZH_HANS("正在下载 LPIPS 检查点 {0}：{1}"),
    ZH_HANT("正在下載 LPIPS 檢查點 {0}：{1}"),
    KO("LPIPS 체크포인트 {0} 내려받는 중: {1}"),
    DE("LPIPS-Checkpoint {0} wird heruntergeladen: {1}"),
    FR("Téléchargement du point de contrôle LPIPS {0} : {1}"),
    ES("Descargando el punto de control de LPIPS {0}: {1}"),
    PT("Baixando o checkpoint do LPIPS {0}: {1}"),
    IT("Download del checkpoint LPIPS {0}: {1}"),
    NL("LPIPS-checkpoint {0} wordt gedownload: {1}"),
    RU("Загрузка контрольной точки LPIPS {0}: {1}"),
    TR("LPIPS denetim noktası {0} indiriliyor: {1}"));

SS_MSG(lpips_fetch_failed,
    EN("Could not download the LPIPS checkpoint {0}, which evaluation needs: {1}. "
       "Turn LPIPS off in the training options to train without it."),
    JA("評価に必要な LPIPS チェックポイント {0} をダウンロードできませんでした: {1}。"
       "LPIPS なしで学習するには、学習オプションで LPIPS をオフにしてください。"),
    ZH_HANS("无法下载评估所需的 LPIPS 检查点 {0}：{1}。若要不用 LPIPS 训练，"
            "请在训练选项中关闭 LPIPS。"),
    ZH_HANT("無法下載評估所需的 LPIPS 檢查點 {0}：{1}。若要不用 LPIPS 訓練，"
            "請在訓練選項中關閉 LPIPS。"),
    KO("평가에 필요한 LPIPS 체크포인트 {0} 파일을 내려받지 못했습니다: {1}. "
       "LPIPS 없이 학습하려면 학습 옵션에서 LPIPS를 끄세요."),
    DE("Der LPIPS-Checkpoint {0}, den die Auswertung braucht, ließ sich nicht "
       "herunterladen: {1}. Um ohne ihn zu trainieren, LPIPS in den "
       "Trainingsoptionen ausschalten."),
    FR("Impossible de télécharger le point de contrôle LPIPS {0}, nécessaire à "
       "l'évaluation : {1}. Pour entraîner sans lui, désactivez LPIPS dans les "
       "options d'entraînement."),
    ES("No se pudo descargar el punto de control de LPIPS {0}, que la evaluación "
       "necesita: {1}. Para entrenar sin él, desactive LPIPS en las opciones de "
       "entrenamiento."),
    PT("Não foi possível baixar o checkpoint do LPIPS {0}, de que a avaliação "
       "precisa: {1}. Para treinar sem ele, desative o LPIPS nas opções de "
       "treinamento."),
    IT("Impossibile scaricare il checkpoint LPIPS {0}, necessario alla "
       "valutazione: {1}. Per addestrare senza, disattiva LPIPS nelle opzioni di "
       "addestramento."),
    NL("Het LPIPS-checkpoint {0}, dat de evaluatie nodig heeft, kon niet worden "
       "gedownload: {1}. Zet LPIPS uit in de trainingsopties om zonder te trainen."),
    RU("Не удалось скачать контрольную точку LPIPS {0}, нужную для оценки: {1}. "
       "Чтобы обучать без неё, отключите LPIPS в параметрах обучения."),
    TR("Değerlendirmenin ihtiyaç duyduğu LPIPS denetim noktası {0} indirilemedi: "
       "{1}. Onsuz eğitmek için eğitim seçeneklerinde LPIPS'i kapatın."));

SS_MSG(pause,
    EN("Pause"),         JA("一時停止"),      ZH_HANS("暂停"),     ZH_HANT("暫停"),
    KO("일시 정지"),      DE("Pause"),        FR("Pause"),        ES("Pausar"),
    PT("Pausar"),        IT("Pausa"),        NL("Pauzeren"),     RU("Пауза"),
    TR("Duraklat"));

SS_MSG(resume,
    EN("Resume"),        JA("再開"),          ZH_HANS("继续"),     ZH_HANT("繼續"),
    KO("이어서"),         DE("Fortsetzen"),   FR("Reprendre"),    ES("Reanudar"),
    PT("Retomar"),       IT("Riprendi"),     NL("Hervatten"),    RU("Продолжить"),
    TR("Sürdür"));

// "&&" is ImGui's escape for a literal ampersand; keep it in every language
// that keeps the ampersand, and drop it where the conjunction is a word.
SS_MSG(stop,
    EN("Stop"),          JA("停止"),          ZH_HANS("停止"),     ZH_HANT("停止"),
    KO("멈춤"),           DE("Anhalten"),     FR("Arrêter"),      ES("Detener"),
    PT("Parar"),         IT("Ferma"),        NL("Stoppen"),      RU("Остановить"),
    TR("Durdur"));

SS_MSG(stop_help,
    EN("Ends the run. Asks first whether to save a final checkpoint; either "
       "way the result stays loaded for viewing."),
    JA("実行を終了します。最後のチェックポイントを保存するかどうかを先に"
       "尋ねます。どちらを選んでも結果は表示用に読み込んだままです。"),
    ZH_HANS("结束这次训练。会先问是否保存最后一个检查点；无论选哪个，结果都"
            "留在内存中以便查看。"),
    ZH_HANT("結束這次訓練。會先問是否儲存最後一個檢查點；無論選哪個，結果都"
            "留在記憶體中以便檢視。"),
    KO("실행을 끝냅니다. 마지막 체크포인트를 저장할지 먼저 묻고, 어느 쪽이든 "
       "결과는 볼 수 있도록 그대로 둡니다."),
    DE("Beendet den Lauf. Fragt vorher, ob ein letzter Prüfpunkt gespeichert "
       "werden soll; in beiden Fällen bleibt das Ergebnis zum Betrachten "
       "geladen."),
    FR("Termine l'exécution. Demande d'abord s'il faut enregistrer un dernier "
       "point de sauvegarde ; dans les deux cas le résultat reste chargé pour "
       "être consulté."),
    ES("Termina la ejecución. Antes pregunta si guardar un último punto de "
       "control; en ambos casos el resultado sigue cargado para verlo."),
    PT("Termina a execução. Antes pergunta se deve salvar um último ponto de "
       "verificação; nos dois casos o resultado continua carregado para "
       "visualização."),
    IT("Termina l'esecuzione. Prima chiede se salvare un ultimo punto di "
       "controllo; in entrambi i casi il risultato resta caricato per "
       "guardarlo."),
    NL("Beëindigt de run. Vraagt eerst of er nog een laatste controlepunt "
       "moet worden opgeslagen; het resultaat blijft in beide gevallen geladen "
       "om te bekijken."),
    RU("Завершает прогон. Сначала спрашивает, сохранять ли последнюю "
       "контрольную точку; в обоих случаях результат остаётся загруженным для "
       "просмотра."),
    TR("Çalışmayı bitirir. Önce son bir denetim noktası kaydedilsin mi diye "
       "sorar; her iki durumda da sonuç görüntülemek için yüklü kalır."));

SS_MSG(stop_without_saving,
    EN("Stop without Saving"),
    JA("保存せずに停止"),  ZH_HANS("不保存并停止"), ZH_HANT("不儲存並停止"),
    KO("저장하지 않고 멈춤"), DE("Anhalten ohne zu speichern"),
    FR("Arrêter sans enregistrer"), ES("Detener sin guardar"),
    PT("Parar sem salvar"), IT("Ferma senza salvare"),
    NL("Stoppen zonder opslaan"), RU("Остановить без сохранения"),
    TR("Kaydetmeden durdur"));

SS_MSG(stop_without_saving_help,
    EN("End the run without writing anything else to disk: what stays in the output folder is what the last automatic save left there. The result is still loaded for viewing."),
    JA("ディスクには何も書き足さずに実行を終了します。出力フォルダに残るのは直前の自動保存までです。結果は表示用に読み込んだままです。"),
    ZH_HANS("结束这次训练，不再往磁盘写任何东西：输出文件夹里留下的仍是上一次自动保存的内容。结果仍留在内存中以便查看。"),
    ZH_HANT("結束這次訓練，不再往磁碟寫任何東西：輸出資料夾裡留下的仍是上一次自動儲存的內容。結果仍留在記憶體中以便檢視。"),
    KO("디스크에 더 쓰지 않고 실행을 끝냅니다. 출력 폴더에 남는 것은 직전 자동 저장까지입니다. 결과는 볼 수 있도록 그대로 둡니다."),
    DE("Beendet ohne weitere Speicherung. Auf der Platte bleibt der letzte automatische Stand; das Ergebnis bleibt zur Ansicht geladen."),
    FR("Arrête sans sauvegarde supplémentaire. Le disque garde la dernière sauvegarde automatique ; le résultat reste chargé."),
    ES("Termina la ejecución sin escribir nada más en el disco: en la carpeta de salida queda lo que dejó el último guardado automático. El resultado sigue cargado para verlo."),
    PT("Termina a execução sem escrever mais nada no disco: na pasta de saída fica o que o último salvamento automático deixou. O resultado continua carregado para visualização."),
    IT("Termina senza altri salvataggi. Su disco resta l’ultimo salvataggio automatico; il risultato rimane caricato."),
    NL("Stopt zonder verder opslaan. Op schijf blijft de laatste automatische opslag; het resultaat blijft geladen."),
    RU("Завершает без новой записи. На диске остаётся последнее автосохранение; результат остаётся загруженным для просмотра."),
    TR("Diske başka bir şey yazmadan çalışmayı bitirir: çıktı klasöründe son otomatik kaydın bıraktığı şey kalır. Sonuç görüntülemek için yüklü kalır."));

SS_MSG(stop_and_save,
    EN("Stop and Save"),  JA("停止して保存"),   ZH_HANS("停止并保存"), ZH_HANT("停止並儲存"),
    KO("멈추고 저장"),    DE("Anhalten und speichern"),
    FR("Arrêter et enregistrer"), ES("Detener y guardar"),
    PT("Parar e salvar"), IT("Ferma e salva"), NL("Stoppen en opslaan"),
    RU("Остановить и сохранить"), TR("Durdur ve kaydet"));

SS_MSG(stopping,
    EN("Stopping..."),   JA("停止しています…"), ZH_HANS("正在停止…"), ZH_HANT("正在停止…"),
    KO("멈추는 중…"),     DE("Wird angehalten …"), FR("Arrêt en cours…"),
    ES("Deteniendo…"),   PT("Parando…"),     IT("Arresto in corso…"),
    NL("Bezig met stoppen…"), RU("Останавливается…"), TR("Durduruluyor…"));

SS_MSG(stop_and_save_help,
    EN("Finishes this step, saves a checkpoint and keeps the result loaded.\nAn unfinished run can resume later with File > Resume Training."),
    JA("このステップを終えて保存し、結果を読み込んだままにします。\n未完了の実行はファイルの「学習を再開」から続行できます。"),
    ZH_HANS("完成当前步骤、保存检查点并保留已加载的结果。\n未完成的运行可从“文件 > 恢复训练”继续。"),
    ZH_HANT("完成目前步驟、儲存檢查點並保留已載入的結果。\n未完成的執行可從「檔案 > 繼續訓練」繼續。"),
    KO("현재 단계를 마치고 체크포인트를 저장하며 결과를 유지합니다.\n미완료 실행은 파일 > 학습 재개에서 이어갈 수 있습니다."),
    DE("Beendet den Schritt, speichert einen Checkpoint und lässt das Ergebnis geladen.\nUnfertige Läufe später über Datei > Training fortsetzen weiterführen."),
    FR("Termine l’étape, sauvegarde un point de contrôle et garde le résultat chargé.\nReprendre une exécution inachevée via Fichier > Reprendre l’entraînement."),
    ES("Termina el paso, guarda un punto de control y mantiene el resultado cargado.\nContinúa una ejecución incompleta con Archivo > Reanudar entrenamiento."),
    PT("Conclui o passo, guarda um checkpoint e mantém o resultado carregado.\nRetome execuções incompletas em Ficheiro > Retomar treino."),
    IT("Termina il passo, salva un checkpoint e mantiene il risultato caricato.\nRiprendi una sessione incompleta da File > Riprendi addestramento."),
    NL("Rondt de stap af, slaat een checkpoint op en houdt het resultaat geladen.\nHervat een onvoltooide run via Bestand > Training hervatten."),
    RU("Завершает шаг, сохраняет контрольную точку и оставляет результат загруженным.\nНезавершённый запуск можно продолжить: Файл > Продолжить обучение."),
    TR("Adımı bitirir, kontrol noktası kaydeder ve sonucu yüklü tutar.\nBitmemiş çalışmayı Dosya > Eğitime Devam Et ile sürdürebilirsiniz."));

SS_MSG(status_step,
    EN("step {0} / {1}  ({2}%)"), JA("ステップ {0} / {1}  ({2}%)"),
    ZH_HANS("第 {0} / {1} 步  ({2}%)"), ZH_HANT("第 {0} / {1} 步  ({2}%)"),
    KO("{0} / {1} 단계  ({2}%)"), DE("Schritt {0} / {1}  ({2}%)"),
    FR("étape {0} / {1}  ({2} %)"), ES("paso {0} / {1}  ({2}%)"),
    PT("passo {0} / {1}  ({2}%)"), IT("passo {0} / {1}  ({2}%)"),
    NL("stap {0} / {1}  ({2}%)"), RU("шаг {0} / {1}  ({2}%)"),
    TR("adım {0} / {1}  (%{2})"));

SS_MSG(status_rate,
    EN("{0} ms/step   elapsed {1}   ETA {2}   splats: {3}"),
    JA("{0} ms/ステップ   経過 {1}   残り {2}   スプラット: {3}"),
    ZH_HANS("{0} 毫秒/步   已用 {1}   剩余 {2}   泼溅数：{3}"),
    ZH_HANT("{0} 毫秒/步   已用 {1}   剩餘 {2}   潑濺數：{3}"),
    KO("{0} ms/단계   경과 {1}   남은 시간 {2}   스플랫: {3}"),
    DE("{0} ms/Schritt   Laufzeit {1}   Restzeit {2}   Splats: {3}"),
    FR("{0} ms/étape   écoulé {1}   reste {2}   splats : {3}"),
    ES("{0} ms/paso   lleva {1}   faltan {2}   splats: {3}"),
    PT("{0} ms/passo   decorrido {1}   faltam {2}   splats: {3}"),
    IT("{0} ms/passo   trascorso {1}   mancano {2}   splat: {3}"),
    NL("{0} ms/stap   verstreken {1}   nog {2}   splats: {3}"),
    RU("{0} мс/шаг   прошло {1}   осталось {2}   сплатов: {3}"),
    TR("{0} ms/adım   geçen {1}   kalan {2}   splat: {3}"));

SS_MSG(status_rate_paused,
    EN("[paused]  {0} ms/step   elapsed {1}   ETA {2}   splats: {3}"),
    JA("［一時停止］  {0} ms/ステップ   経過 {1}   残り {2}   スプラット: {3}"),
    ZH_HANS("［已暂停］  {0} 毫秒/步   已用 {1}   剩余 {2}   泼溅数：{3}"),
    ZH_HANT("［已暫停］  {0} 毫秒/步   已用 {1}   剩餘 {2}   潑濺數：{3}"),
    KO("[일시 정지]  {0} ms/단계   경과 {1}   남은 시간 {2}   스플랫: {3}"),
    DE("[pausiert]  {0} ms/Schritt   Laufzeit {1}   Restzeit {2}   Splats: {3}"),
    FR("[en pause]  {0} ms/étape   écoulé {1}   reste {2}   splats : {3}"),
    ES("[en pausa]  {0} ms/paso   lleva {1}   faltan {2}   splats: {3}"),
    PT("[pausado]  {0} ms/passo   decorrido {1}   faltam {2}   splats: {3}"),
    IT("[in pausa]  {0} ms/passo   trascorso {1}   mancano {2}   splat: {3}"),
    NL("[gepauzeerd]  {0} ms/stap   verstreken {1}   nog {2}   splats: {3}"),
    RU("[пауза]  {0} мс/шаг   прошло {1}   осталось {2}   сплатов: {3}"),
    TR("[duraklatıldı]  {0} ms/adım   geçen {1}   kalan {2}   splat: {3}"));

SS_MSG(status_done_steps,
    EN("done ({0} steps in {1})"),
    JA("完了（{0} ステップ、所要 {1}）"),
    ZH_HANS("完成（{0} 步，用时 {1}）"),
    ZH_HANT("完成（{0} 步，用時 {1}）"),
    KO("완료({0} 단계, {1} 소요)"),
    DE("fertig ({0} Schritte in {1})"),
    FR("terminé ({0} étapes en {1})"),
    ES("terminado ({0} pasos en {1})"),
    PT("concluído ({0} passos em {1})"),
    IT("completato ({0} passi in {1})"),
    NL("klaar ({0} stappen in {1})"),
    RU("готово (шагов: {0}, время: {1})"),
    TR("bitti ({0} adım, {1})"));

SS_MSG(status_explore,
    EN("explore the result in the viewport above"),
    JA("上のビューポートで結果を見てまわれます"),
    ZH_HANS("可以在上方视口中查看结果"),
    ZH_HANT("可以在上方檢視區中查看結果"),
    KO("위쪽 뷰포트에서 결과를 둘러보세요"),
    DE("Das Ergebnis lässt sich im Fenster darüber erkunden"),
    FR("explorez le résultat dans la vue ci-dessus"),
    ES("explore el resultado en la vista de arriba"),
    PT("explore o resultado na visualização acima"),
    IT("esplori il risultato nella vista qui sopra"),
    NL("bekijk het resultaat in het beeld hierboven"),
    RU("результат можно осмотреть в окне выше"),
    TR("sonucu yukarıdaki görünümde gezebilirsiniz"));

SS_MSG(status_preparing,
    EN("preparing"),     JA("準備中"),        ZH_HANS("准备中"),   ZH_HANT("準備中"),
    KO("준비 중"),        DE("wird vorbereitet"), FR("préparation"),
    ES("preparando"),    PT("preparando"),   IT("preparazione"),
    NL("bezig met voorbereiden"), RU("подготовка"), TR("hazırlanıyor"));

SS_MSG(status_evaluating,
    EN("evaluating"),     JA("評価中"),         ZH_HANS("评估中"),  ZH_HANT("評估中"),
    KO("평가 중"),         DE("Auswertung läuft"), FR("évaluation"),
    ES("evaluando"),      PT("avaliando"),      IT("valutazione in corso"),
    NL("bezig met evalueren"), RU("оценка"),     TR("değerlendiriliyor"));

SS_MSG(status_eval_views,
    EN("evaluating {0}/{1} views ({2}%)"),
    JA("評価中 {0}/{1} 視点 ({2}%)"),
    ZH_HANS("正在评估 {0}/{1} 个视角 ({2}%)"),
    ZH_HANT("正在評估 {0}/{1} 個視角 ({2}%)"),
    KO("평가 중 {0}/{1} 뷰 ({2}%)"),
    DE("Auswertung {0}/{1} Ansichten ({2}%)"),
    FR("évaluation {0}/{1} vues ({2}%)"),
    ES("evaluando {0}/{1} vistas ({2}%)"),
    PT("avaliando {0}/{1} vistas ({2}%)"),
    IT("valutazione {0}/{1} viste ({2}%)"),
    NL("bezig met evalueren {0}/{1} weergaven ({2}%)"),
    RU("оценка: {0}/{1} ракурсов ({2}%)"),
    TR("{0}/{1} görüntü değerlendiriliyor (%{2})"));

SS_MSG(status_ready,
    EN("ready"),         JA("準備完了"),      ZH_HANS("就绪"),     ZH_HANT("就緒"),
    KO("준비됨"),         DE("bereit"),       FR("prêt"),         ES("listo"),
    PT("pronto"),        IT("pronto"),       NL("gereed"),       RU("готово"),
    TR("hazır"));

SS_MSG(status_ready_hint,
    EN("dataset preview -- press Start Training when ready"),
    JA("データセットのプレビューです。よければ「学習を開始」を押してください"),
    ZH_HANS("这是数据集预览——准备好后请按“开始训练”"),
    ZH_HANT("這是資料集預覽——準備好後請按「開始訓練」"),
    KO("데이터셋 미리보기입니다. 준비되면 [학습 시작]을 누르세요"),
    DE("Datensatzvorschau -- wenn es passt, auf „Training starten“ drücken"),
    FR("aperçu du jeu de données -- appuyez sur « Lancer l'entraînement » "
       "quand vous êtes prêt"),
    ES("vista previa del conjunto de datos: pulse «Iniciar el entrenamiento» "
       "cuando esté listo"),
    PT("prévia do conjunto de dados -- pressione “Iniciar o treinamento” "
       "quando estiver pronto"),
    IT("anteprima del set di dati -- prema «Avvia l'addestramento» quando è "
       "pronto"),
    NL("voorbeeld van de dataset -- druk op ‘Training starten’ als het goed is"),
    RU("предпросмотр набора данных — нажмите «Начать обучение», когда будете "
       "готовы"),
    TR("veri kümesi önizlemesi -- hazır olduğunuzda “Eğitimi başlat”a basın"));

SS_MSG(status_idle,
    EN("idle"),          JA("待機中"),        ZH_HANS("空闲"),     ZH_HANT("閒置"),
    KO("대기 중"),        DE("bereit"),       FR("inactif"),      ES("inactivo"),
    PT("ocioso"),        IT("inattivo"),     NL("inactief"),     RU("ожидание"),
    TR("boşta"));

// The metric names are the ones the literature and the logs use; only the
// labelling around them changes.
SS_MSG(metric_loss,
    EN("Loss"), JA("損失"), ZH_HANS("损失"), ZH_HANT("損失"),
    KO("손실"), DE("Verlust"), FR("Perte"), ES("Pérdida"),
    PT("Perda"), IT("Perdita"), NL("Verlies"), RU("Потери"), TR("Kayıp"));

SS_MSG(metric_loss_log,
    EN("Loss (log)"), JA("損失 (対数)"), ZH_HANS("损失 (对数)"), ZH_HANT("損失 (對數)"),
    KO("손실 (로그)"), DE("Verlust (log)"), FR("Perte (log)"), ES("Pérdida (log)"),
    PT("Perda (log)"), IT("Perdita (log)"), NL("Verlies (log)"), RU("Потери (лог)"),
    TR("Kayıp (log)"));

SS_MSG(status_metrics,
    EN("splats: {0}   psnr: {1}   ssim: {2}   loss: {3}"),
    JA("スプラット: {0}   psnr: {1}   ssim: {2}   損失: {3}"),
    ZH_HANS("泼溅数：{0}   psnr: {1}   ssim：{2}   损失：{3}"),
    ZH_HANT("潑濺數：{0}   psnr: {1}   ssim：{2}   損失：{3}"),
    KO("스플랫: {0}   psnr: {1}   ssim: {2}   손실: {3}"),
    DE("Splats: {0}   psnr: {1}   ssim: {2}   Verlust: {3}"),
    FR("splats : {0}   psnr: {1}   ssim : {2}   perte : {3}"),
    ES("splats: {0}   psnr: {1}   ssim: {2}   pérdida: {3}"),
    PT("splats: {0}   psnr: {1}   ssim: {2}   perda: {3}"),
    IT("splat: {0}   psnr: {1}   ssim: {2}   perdita: {3}"),
    NL("splats: {0}   psnr: {1}   ssim: {2}   verlies: {3}"),
    RU("сплатов: {0}   psnr: {1}   ssim: {2}   потери: {3}"),
    TR("splat: {0}   psnr: {1}   ssim: {2}   kayıp: {3}"));

SS_MSG(vram_help,
    EN("GPU memory (GiB): used by this process / total in use system-wide / "
       "device capacity. '?' means the backend could not query that value."),
    JA("GPU メモリ（GiB）: このプロセスの使用量 / システム全体の使用量 / "
       "デバイスの容量。「?」はバックエンドがその値を取得できなかったことを"
       "示します。"),
    ZH_HANS("显存（GiB）：本进程占用 / 系统整体占用 / 设备容量。“?”表示后端"
            "无法查询到该数值。"),
    ZH_HANT("顯示記憶體（GiB）：本行程佔用 / 系統整體佔用 / 裝置容量。「?」表示"
            "後端無法查詢到該數值。"),
    KO("GPU 메모리(GiB): 이 프로세스 사용량 / 시스템 전체 사용량 / 장치 용량. "
       "'?'는 백엔드가 그 값을 조회하지 못했다는 뜻입니다."),
    DE("Grafikspeicher (GiB): von diesem Prozess belegt / systemweit belegt / "
       "Kapazität des Geräts. „?“ heißt, dass das Backend den Wert nicht "
       "abfragen konnte."),
    FR("Mémoire GPU (Gio) : utilisée par ce processus / utilisée à l'échelle "
       "du système / capacité du périphérique. « ? » signifie que le backend "
       "n'a pas pu obtenir la valeur."),
    ES("Memoria de GPU (GiB): usada por este proceso / usada en todo el "
       "sistema / capacidad del dispositivo. «?» significa que el backend no "
       "pudo consultar ese valor."),
    PT("Memória da GPU (GiB): usada por este processo / usada em todo o "
       "sistema / capacidade do dispositivo. “?” significa que o backend não "
       "conseguiu consultar esse valor."),
    IT("Memoria GPU (GiB): usata da questo processo / usata a livello di "
       "sistema / capacità del dispositivo. «?» significa che il backend non "
       "è riuscito a leggere quel valore."),
    NL("GPU-geheugen (GiB): in gebruik door dit proces / systeembreed in "
       "gebruik / capaciteit van het apparaat. ‘?’ betekent dat de backend de "
       "waarde niet kon opvragen."),
    RU("Видеопамять (ГиБ): занято этим процессом / занято во всей системе / "
       "объём устройства. «?» означает, что бэкенд не смог получить значение."),
    TR("GPU belleği (GiB): bu sürecin kullandığı / sistem genelinde kullanılan "
       "/ aygıtın kapasitesi. “?”, arka ucun o değeri sorgulayamadığı "
       "anlamına gelir."));

// The VRAM forecast: the risk tag beside the bar, and its hover card.
SS_MSG(oom_risk_low,
    EN("OOM risk: low"), JA("メモリ不足リスク: 低"), ZH_HANS("显存不足风险：低"),
    ZH_HANT("顯示記憶體不足風險：低"), KO("메모리 부족 위험: 낮음"),
    DE("OOM-Risiko: gering"), FR("Risque de saturation : faible"),
    ES("Riesgo de falta de memoria: bajo"), PT("Risco de falta de memória: baixo"),
    IT("Rischio memoria esaurita: basso"), NL("Risico geheugentekort: laag"),
    RU("Риск нехватки памяти: низкий"), TR("Bellek yetmeme riski: düşük"));

SS_MSG(oom_risk_medium,
    EN("OOM risk: medium"), JA("メモリ不足リスク: 中"), ZH_HANS("显存不足风险：中"),
    ZH_HANT("顯示記憶體不足風險：中"), KO("메모리 부족 위험: 보통"),
    DE("OOM-Risiko: mittel"), FR("Risque de saturation : moyen"),
    ES("Riesgo de falta de memoria: medio"), PT("Risco de falta de memória: médio"),
    IT("Rischio memoria esaurita: medio"), NL("Risico geheugentekort: middel"),
    RU("Риск нехватки памяти: средний"), TR("Bellek yetmeme riski: orta"));

SS_MSG(oom_risk_high,
    EN("OOM risk: high"), JA("メモリ不足リスク: 高"), ZH_HANS("显存不足风险：高"),
    ZH_HANT("顯示記憶體不足風險：高"), KO("메모리 부족 위험: 높음"),
    DE("OOM-Risiko: hoch"), FR("Risque de saturation : élevé"),
    ES("Riesgo de falta de memoria: alto"), PT("Risco de falta de memória: alto"),
    IT("Rischio memoria esaurita: alto"), NL("Risico geheugentekort: hoog"),
    RU("Риск нехватки памяти: высокий"), TR("Bellek yetmeme riski: yüksek"));

SS_MSG(vram_chart_title,
    EN("GPU memory over the run (GiB)"), JA("学習中の GPU メモリ（GiB）"),
    ZH_HANS("训练过程中的显存（GiB）"), ZH_HANT("訓練過程中的顯示記憶體（GiB）"),
    KO("학습 중 GPU 메모리(GiB)"), DE("Grafikspeicher im Verlauf (GiB)"),
    FR("Mémoire GPU au fil de l'entraînement (Gio)"),
    ES("Memoria de GPU durante el entrenamiento (GiB)"),
    PT("Memória da GPU ao longo do treinamento (GiB)"),
    IT("Memoria GPU durante l'addestramento (GiB)"),
    NL("GPU-geheugen tijdens de training (GiB)"),
    RU("Видеопамять по ходу обучения (ГиБ)"), TR("Eğitim boyunca GPU belleği (GiB)"));

SS_MSG(vram_legend_run,
    EN("this run"), JA("この学習"), ZH_HANS("本次训练"), ZH_HANT("本次訓練"),
    KO("이 학습"), DE("dieses Training"), FR("cet entraînement"),
    ES("este entrenamiento"), PT("este treinamento"), IT("questo addestramento"),
    NL("deze training"), RU("это обучение"), TR("bu eğitim"));

SS_MSG(vram_legend_projected,
    EN("projected (95% band)"), JA("予測（95% 範囲）"), ZH_HANS("预测（95% 区间）"),
    ZH_HANT("預測（95% 區間）"), KO("예측(95% 범위)"), DE("Prognose (95-%-Band)"),
    FR("prévision (bande à 95 %)"), ES("previsión (banda del 95 %)"),
    PT("previsão (faixa de 95%)"), IT("previsione (banda al 95%)"),
    NL("prognose (95%-band)"), RU("прогноз (полоса 95 %)"), TR("tahmin (%95 aralığı)"));

SS_MSG(vram_legend_others,
    EN("other programs"), JA("他のプログラム"), ZH_HANS("其他程序"), ZH_HANT("其他程式"),
    KO("다른 프로그램"), DE("andere Programme"), FR("autres programmes"),
    ES("otros programas"), PT("outros programas"), IT("altri programmi"),
    NL("andere programma's"), RU("другие программы"), TR("diğer programlar"));

SS_MSG(vram_legend_capacity,
    EN("device capacity"), JA("デバイスの容量"), ZH_HANS("设备容量"), ZH_HANT("裝置容量"),
    KO("장치 용량"), DE("Kapazität des Geräts"), FR("capacité du périphérique"),
    ES("capacidad del dispositivo"), PT("capacidade do dispositivo"),
    IT("capacità del dispositivo"), NL("capaciteit van het apparaat"),
    RU("объём устройства"), TR("aygıt kapasitesi"));

SS_MSG(vram_chart_peak,
    EN("Projected peak: {0} ± {1} GiB   free for training: {2} GiB   chance of running out: {3}%"),
    JA("予測ピーク: {0} ± {1} GiB   学習に使える量: {2} GiB   不足する確率: {3}%"),
    ZH_HANS("预计峰值：{0} ± {1} GiB   可供训练：{2} GiB   耗尽的概率：{3}%"),
    ZH_HANT("預計峰值：{0} ± {1} GiB   可供訓練：{2} GiB   耗盡的機率：{3}%"),
    KO("예상 최대치: {0} ± {1} GiB   학습에 쓸 수 있는 양: {2} GiB   부족할 확률: {3}%"),
    DE("Erwartete Spitze: {0} ± {1} GiB   für das Training frei: {2} GiB   Wahrscheinlichkeit, dass er ausgeht: {3} %"),
    FR("Pic prévu : {0} ± {1} Gio   disponible pour l'entraînement : {2} Gio   probabilité de saturation : {3} %"),
    ES("Pico previsto: {0} ± {1} GiB   libre para entrenar: {2} GiB   probabilidad de quedarse sin memoria: {3} %"),
    PT("Pico previsto: {0} ± {1} GiB   livre para o treinamento: {2} GiB   chance de faltar memória: {3}%"),
    IT("Picco previsto: {0} ± {1} GiB   libera per l'addestramento: {2} GiB   probabilità di esaurirla: {3}%"),
    NL("Verwachte piek: {0} ± {1} GiB   vrij voor training: {2} GiB   kans op tekort: {3}%"),
    RU("Ожидаемый пик: {0} ± {1} ГиБ   доступно для обучения: {2} ГиБ   вероятность нехватки: {3} %"),
    TR("Beklenen tepe: {0} ± {1} GiB   eğitim için boş: {2} GiB   yetmeme olasılığı: %{3}"));

SS_MSG(vram_chart_waiting,
    EN("The projection appears after the first steps have been measured."),
    JA("最初のステップを計測すると予測が表示されます。"),
    ZH_HANS("测量完最初的若干步后会显示预测。"),
    ZH_HANT("量測完最初的若干步後會顯示預測。"),
    KO("처음 몇 단계를 측정하면 예측이 표시됩니다."),
    DE("Die Prognose erscheint, sobald die ersten Schritte gemessen sind."),
    FR("La prévision apparaît une fois les premières étapes mesurées."),
    ES("La previsión aparece cuando se han medido los primeros pasos."),
    PT("A previsão aparece depois que os primeiros passos são medidos."),
    IT("La previsione compare dopo che i primi passi sono stati misurati."),
    NL("De prognose verschijnt zodra de eerste stappen gemeten zijn."),
    RU("Прогноз появится, когда будут измерены первые шаги."),
    TR("Tahmin, ilk adımlar ölçüldükten sonra görünür."));

SS_MSG(vram_breakdown_title,
    EN("This run by category (GiB)"), JA("この学習の内訳（GiB）"),
    ZH_HANS("本次训练按类别（GiB）"), ZH_HANT("本次訓練按類別（GiB）"),
    KO("이 학습의 항목별 사용량(GiB)"), DE("Dieses Training nach Kategorie (GiB)"),
    FR("Cet entraînement par catégorie (Gio)"), ES("Este entrenamiento por categoría (GiB)"),
    PT("Este treinamento por categoria (GiB)"), IT("Questo addestramento per categoria (GiB)"),
    NL("Deze training per categorie (GiB)"), RU("Это обучение по категориям (ГиБ)"),
    TR("Bu eğitim, kategoriye göre (GiB)"));

SS_MSG(vram_breakdown_growth,
    EN("Faded: growth still to come, up to the projected peak."),
    JA("薄い部分: 予測ピークまでにこれから増える分。"),
    ZH_HANS("浅色部分：到预计峰值前还会增加的量。"),
    ZH_HANT("淺色部分：到預計峰值前還會增加的量。"),
    KO("흐린 부분: 예상 최대치까지 앞으로 늘어날 양."),
    DE("Blass: der Zuwachs, der bis zur erwarteten Spitze noch kommt."),
    FR("En pâle : la croissance encore à venir, jusqu'au pic prévu."),
    ES("Atenuado: el crecimiento que aún falta hasta el pico previsto."),
    PT("Esmaecido: o crescimento que ainda virá, até o pico previsto."),
    IT("Sbiadito: la crescita ancora da venire, fino al picco previsto."),
    NL("Vaag: de groei die nog komt, tot de verwachte piek."),
    RU("Бледным: рост, который ещё впереди, до ожидаемого пика."),
    TR("Soluk: beklenen tepeye kadar daha gelecek artış."));

SS_MSG(vram_cat_splat,
    EN("Splats"), JA("スプラット"), ZH_HANS("泼溅"), ZH_HANT("潑濺"), KO("스플랫"),
    DE("Splats"), FR("Splats"), ES("Splats"), PT("Splats"), IT("Splat"),
    NL("Splats"), RU("Сплаты"), TR("Splat'ler"));

SS_MSG(vram_cat_splat_x_img,
    EN("Splats × images"), JA("スプラット × 画像"), ZH_HANS("泼溅 × 图像"),
    ZH_HANT("潑濺 × 影像"), KO("스플랫 × 이미지"), DE("Splats × Bilder"),
    FR("Splats × images"), ES("Splats × imágenes"), PT("Splats × imagens"),
    IT("Splat × immagini"), NL("Splats × beelden"), RU("Сплаты × изображения"),
    TR("Splat × görüntü"));

SS_MSG(vram_cat_image,
    EN("Images"), JA("画像"), ZH_HANS("图像"), ZH_HANT("影像"), KO("이미지"),
    DE("Bilder"), FR("Images"), ES("Imágenes"), PT("Imagens"), IT("Immagini"),
    NL("Beelden"), RU("Изображения"), TR("Görüntüler"));

SS_MSG(vram_cat_appearance,
    EN("Appearance"), JA("外観補正"), ZH_HANS("外观校正"), ZH_HANT("外觀校正"),
    KO("외관 보정"), DE("Erscheinungsbild"), FR("Apparence"), ES("Apariencia"),
    PT("Aparência"), IT("Aspetto"), NL("Uiterlijk"), RU("Внешний вид"),
    TR("Görünüm"));

SS_MSG(vram_cat_viewer,
    EN("Viewer"), JA("ビューア"), ZH_HANS("查看器"), ZH_HANT("檢視器"), KO("뷰어"),
    DE("Betrachter"), FR("Visionneuse"), ES("Visor"), PT("Visualizador"),
    IT("Visualizzatore"), NL("Viewer"), RU("Просмотр"), TR("Görüntüleyici"));

SS_MSG(vram_cat_other,
    EN("Other"), JA("その他"), ZH_HANS("其他"), ZH_HANT("其他"), KO("기타"),
    DE("Sonstiges"), FR("Autre"), ES("Otros"), PT("Outros"), IT("Altro"),
    NL("Overig"), RU("Прочее"), TR("Diğer"));

SS_MSG(vram_cat_scratch,
    EN("Sort scratch"), JA("ソート用の作業領域"), ZH_HANS("排序临时缓冲"),
    ZH_HANT("排序暫存緩衝"), KO("정렬 작업 공간"), DE("Sortierpuffer"),
    FR("Tampon de tri"), ES("Búfer de ordenación"), PT("Buffer de ordenação"),
    IT("Buffer di ordinamento"), NL("Sorteerbuffer"), RU("Буфер сортировки"),
    TR("Sıralama tamponu"));

SS_MSG(vram_cat_unpooled,
    EN("Backend and staging"), JA("バックエンドと転送用"), ZH_HANS("后端与中转"),
    ZH_HANT("後端與中轉"), KO("백엔드와 전송용"), DE("Backend und Staging"),
    FR("Backend et transfert"), ES("Backend y transferencia"),
    PT("Backend e transferência"), IT("Backend e trasferimento"),
    NL("Backend en staging"), RU("Бэкенд и передача"), TR("Arka uç ve aktarım"));

// ===========================================================================
// Step timing breakdown
// ===========================================================================

SS_MSG(timing_details,
    EN("details"),
    JA("詳細"),
    ZH_HANS("详情"),
    ZH_HANT("詳情"),
    KO("자세히"),
    DE("Details"),
    FR("détails"),
    ES("detalles"),
    PT("detalhes"),
    IT("dettagli"),
    NL("details"),
    RU("подробнее"),
    TR("ayrıntılar"));

SS_MSG(timing_title,
    EN("Where the time goes"),
    JA("時間の内訳"),
    ZH_HANS("时间花在哪里"),
    ZH_HANT("時間花在哪裡"),
    KO("시간이 쓰이는 곳"),
    DE("Wohin die Zeit geht"),
    FR("Où passe le temps"),
    ES("En qué se va el tiempo"),
    PT("Para onde vai o tempo"),
    IT("Dove va il tempo"),
    NL("Waar de tijd heen gaat"),
    RU("На что уходит время"),
    TR("Zaman nereye gidiyor"));

SS_MSG(timing_step_total,
    EN("Average step: {0} ms   Steps averaged: {1}"),
    JA("平均ステップ: {0} ms   平均したステップ数: {1}"),
    ZH_HANS("平均每步：{0} 毫秒   平均步数：{1}"),
    ZH_HANT("平均每步：{0} 毫秒   平均步數：{1}"),
    KO("평균 단계: {0} ms   평균한 단계 수: {1}"),
    DE("Mittlerer Schritt: {0} ms   Gemittelte Schritte: {1}"),
    FR("Étape moyenne : {0} ms   Étapes moyennées : {1}"),
    ES("Paso medio: {0} ms   Pasos promediados: {1}"),
    PT("Passo médio: {0} ms   Passos na média: {1}"),
    IT("Passo medio: {0} ms   Passi mediati: {1}"),
    NL("Gemiddelde stap: {0} ms   Stappen in het gemiddelde: {1}"),
    RU("Средний шаг: {0} мс   Шагов в среднем: {1}"),
    TR("Ortalama adım: {0} ms   Ortalaması alınan adım: {1}"));

SS_MSG(timing_run_total,
    EN("This run: {0}, measured so far, forecast for the rest"),
    JA("この学習: {0}（ここまでは実測、残りは予測）"),
    ZH_HANS("本次训练：{0}（已完成部分为实测，其余为预测）"),
    ZH_HANT("本次訓練：{0}（已完成部分為實測，其餘為預測）"),
    KO("이 학습: {0} (지금까지는 측정값, 나머지는 예측값)"),
    DE("Dieses Training: {0}, bisher gemessen, der Rest geschätzt"),
    FR("Cet entraînement : {0}, mesuré jusqu'ici, prévu pour la suite"),
    ES("Este entrenamiento: {0}, medido hasta ahora, previsto el resto"),
    PT("Este treinamento: {0}, medido até agora, previsto o resto"),
    IT("Questo addestramento: {0}, misurato finora, previsto il resto"),
    NL("Deze training: {0}, tot nu toe gemeten, de rest voorspeld"),
    RU("Это обучение: {0}, измерено до сих пор, остальное — прогноз"),
    TR("Bu eğitim: {0}, şimdiye kadar ölçülen, kalanı tahmin"));

SS_MSG(timing_mode_step,
    EN("Per step"),
    JA("1ステップあたり"),
    ZH_HANS("每步"),
    ZH_HANT("每步"),
    KO("단계당"),
    DE("Pro Schritt"),
    FR("Par étape"),
    ES("Por paso"),
    PT("Por passo"),
    IT("Per passo"),
    NL("Per stap"),
    RU("На шаг"),
    TR("Adım başına"));

SS_MSG(timing_mode_run,
    EN("Whole run"),
    JA("学習全体"),
    ZH_HANS("整个训练"),
    ZH_HANT("整個訓練"),
    KO("전체 학습"),
    DE("Ganzes Training"),
    FR("Entraînement complet"),
    ES("Entrenamiento completo"),
    PT("Treinamento completo"),
    IT("Addestramento completo"),
    NL("Hele training"),
    RU("Всё обучение"),
    TR("Tüm eğitim"));

SS_MSG(timing_view_lists,
    EN("Lists"),
    JA("リスト"),
    ZH_HANS("列表"),
    ZH_HANT("列表"),
    KO("목록"),
    DE("Listen"),
    FR("Listes"),
    ES("Listas"),
    PT("Listas"),
    IT("Elenchi"),
    NL("Lijsten"),
    RU("Списки"),
    TR("Listeler"));

SS_MSG(timing_view_grid,
    EN("Grid"),
    JA("グリッド"),
    ZH_HANS("网格"),
    ZH_HANT("網格"),
    KO("격자"),
    DE("Raster"),
    FR("Grille"),
    ES("Cuadrícula"),
    PT("Grade"),
    IT("Griglia"),
    NL("Raster"),
    RU("Сетка"),
    TR("Izgara"));

SS_MSG(timing_by_part,
    EN("What it is for"),
    JA("用途"),
    ZH_HANS("用途"),
    ZH_HANT("用途"),
    KO("용도"),
    DE("Wofür"),
    FR("À quoi il sert"),
    ES("Para qué"),
    PT("Para quê"),
    IT("A cosa serve"),
    NL("Waarvoor"),
    RU("На что"),
    TR("Ne için"));

SS_MSG(timing_by_scale,
    EN("What it grows with"),
    JA("何に比例するか"),
    ZH_HANS("随什么增长"),
    ZH_HANT("隨什麼增長"),
    KO("무엇에 비례하는지"),
    DE("Womit es wächst"),
    FR("Ce qui le fait croître"),
    ES("Con qué crece"),
    PT("Com o que cresce"),
    IT("Con cosa cresce"),
    NL("Waarmee het groeit"),
    RU("От чего растёт"),
    TR("Neyle büyür"));

SS_MSG(timing_waiting,
    EN("Shown once the first steps have been timed."),
    JA("最初のステップを計測すると表示されます。"),
    ZH_HANS("计时完最初几步后显示。"),
    ZH_HANT("計時完最初幾步後顯示。"),
    KO("처음 몇 단계를 측정하면 표시됩니다."),
    DE("Erscheint, sobald die ersten Schritte gemessen sind."),
    FR("S'affiche dès que les premières étapes sont mesurées."),
    ES("Aparece cuando se han medido los primeros pasos."),
    PT("Aparece depois que os primeiros passos são medidos."),
    IT("Compare dopo che i primi passi sono stati misurati."),
    NL("Verschijnt zodra de eerste stappen gemeten zijn."),
    RU("Появится, когда будут измерены первые шаги."),
    TR("İlk adımlar ölçüldükten sonra görünür."));

SS_MSG(timing_grid_hint,
    EN("Row heights and column widths follow their share of the time."),
    JA("行の高さと列の幅は時間の割合に比例します。"),
    ZH_HANS("行高和列宽与其所占时间成正比。"),
    ZH_HANT("列高與欄寬與其所占時間成正比。"),
    KO("행 높이와 열 너비는 시간 비중에 비례합니다."),
    DE("Zeilenhöhe und Spaltenbreite folgen ihrem Anteil an der Zeit."),
    FR("La hauteur des lignes et la largeur des colonnes suivent leur part du temps."),
    ES("La altura de las filas y el ancho de las columnas siguen su parte del tiempo."),
    PT("A altura das linhas e a largura das colunas seguem sua parte do tempo."),
    IT("Altezza delle righe e larghezza delle colonne seguono la loro quota di tempo."),
    NL("Rijhoogte en kolombreedte volgen hun aandeel in de tijd."),
    RU("Высота строк и ширина столбцов пропорциональны их доле времени."),
    TR("Satır yükseklikleri ve sütun genişlikleri zaman paylarını izler."));

SS_MSG(timing_click_hint,
    EN("Click to pin this and switch views."),
    JA("クリックすると固定して表示を切り替えられます。"),
    ZH_HANS("点击可固定并切换视图。"),
    ZH_HANT("點擊可固定並切換檢視。"),
    KO("클릭하면 고정하고 보기를 바꿀 수 있습니다."),
    DE("Klicken, um dies anzuheften und die Ansicht zu wechseln."),
    FR("Cliquez pour l'épingler et changer de vue."),
    ES("Haz clic para fijarlo y cambiar de vista."),
    PT("Clique para fixar e trocar a visualização."),
    IT("Fai clic per fissarlo e cambiare vista."),
    NL("Klik om dit vast te zetten en van weergave te wisselen."),
    RU("Нажмите, чтобы закрепить и переключить вид."),
    TR("Sabitlemek ve görünümü değiştirmek için tıklayın."));

SS_MSG(timing_part_render,
    EN("Rendering"),
    JA("レンダリング"),
    ZH_HANS("渲染"),
    ZH_HANT("渲染"),
    KO("렌더링"),
    DE("Rendern"),
    FR("Rendu"),
    ES("Renderizado"),
    PT("Renderização"),
    IT("Rendering"),
    NL("Renderen"),
    RU("Рендеринг"),
    TR("İşleme"));

SS_MSG(timing_part_backward,
    EN("Backpropagation"),
    JA("逆伝播"),
    ZH_HANS("反向传播"),
    ZH_HANT("反向傳播"),
    KO("역전파"),
    DE("Rückpropagation"),
    FR("Rétropropagation"),
    ES("Retropropagación"),
    PT("Retropropagação"),
    IT("Retropropagazione"),
    NL("Terugpropagatie"),
    RU("Обратное распространение"),
    TR("Geri yayılım"));

SS_MSG(timing_part_loss,
    EN("Loss"),
    JA("損失"),
    ZH_HANS("损失"),
    ZH_HANT("損失"),
    KO("손실"),
    DE("Verlust"),
    FR("Perte"),
    ES("Pérdida"),
    PT("Perda"),
    IT("Perdita"),
    NL("Verlies"),
    RU("Функция потерь"),
    TR("Kayıp"));

SS_MSG(timing_part_optimizer,
    EN("Optimizer"),
    JA("オプティマイザ"),
    ZH_HANS("优化器"),
    ZH_HANT("最佳化器"),
    KO("옵티마이저"),
    DE("Optimierer"),
    FR("Optimiseur"),
    ES("Optimizador"),
    PT("Otimizador"),
    IT("Ottimizzatore"),
    NL("Optimizer"),
    RU("Оптимизатор"),
    TR("Optimize edici"));

SS_MSG(timing_part_densify,
    EN("Densification"),
    JA("スプラット追加"),
    ZH_HANS("增密"),
    ZH_HANT("增密"),
    KO("밀도화"),
    DE("Verdichtung"),
    FR("Densification"),
    ES("Densificación"),
    PT("Densificação"),
    IT("Densificazione"),
    NL("Verdichting"),
    RU("Уплотнение"),
    TR("Yoğunlaştırma"));

SS_MSG(timing_part_calibration,
    EN("Calibration correction"),
    JA("キャリブレーション補正"),
    ZH_HANS("标定校正"),
    ZH_HANT("標定校正"),
    KO("캘리브레이션 보정"),
    DE("Kalibrierkorrektur"),
    FR("Correction d'étalonnage"),
    ES("Corrección de calibración"),
    PT("Correção de calibração"),
    IT("Correzione della calibrazione"),
    NL("Kalibratiecorrectie"),
    RU("Коррекция калибровки"),
    TR("Kalibrasyon düzeltmesi"));

SS_MSG(timing_part_geometry,
    EN("Depth and normals"),
    JA("深度と法線"),
    ZH_HANS("深度与法线"),
    ZH_HANT("深度與法線"),
    KO("깊이와 법선"),
    DE("Tiefe und Normalen"),
    FR("Profondeur et normales"),
    ES("Profundidad y normales"),
    PT("Profundidade e normais"),
    IT("Profondità e normali"),
    NL("Diepte en normalen"),
    RU("Глубина и нормали"),
    TR("Derinlik ve normaller"));

SS_MSG(timing_part_masks,
    EN("Masks"),
    JA("マスク"),
    ZH_HANS("蒙版"),
    ZH_HANT("遮罩"),
    KO("마스크"),
    DE("Masken"),
    FR("Masques"),
    ES("Máscaras"),
    PT("Máscaras"),
    IT("Maschere"),
    NL("Maskers"),
    RU("Маски"),
    TR("Maskeler"));

SS_MSG(timing_part_warp,
    EN("Fisheye split"),
    JA("魚眼の分割"),
    ZH_HANS("鱼眼拆分"),
    ZH_HANT("魚眼拆分"),
    KO("어안 분할"),
    DE("Fischaugen-Aufteilung"),
    FR("Découpage du fisheye"),
    ES("División del ojo de pez"),
    PT("Divisão do olho de peixe"),
    IT("Divisione del fisheye"),
    NL("Fisheye-opsplitsing"),
    RU("Разбиение фишая"),
    TR("Balıkgözü bölme"));

SS_MSG(timing_part_color_space,
    EN("Color space"),
    JA("色空間"),
    ZH_HANS("色彩空间"),
    ZH_HANT("色彩空間"),
    KO("색 공간"),
    DE("Farbraum"),
    FR("Espace colorimétrique"),
    ES("Espacio de color"),
    PT("Espaço de cor"),
    IT("Spazio colore"),
    NL("Kleurruimte"),
    RU("Цветовое пространство"),
    TR("Renk uzayı"));

SS_MSG(timing_part_background,
    EN("Background"),
    JA("背景"),
    ZH_HANS("背景"),
    ZH_HANT("背景"),
    KO("배경"),
    DE("Hintergrund"),
    FR("Arrière-plan"),
    ES("Fondo"),
    PT("Fundo"),
    IT("Sfondo"),
    NL("Achtergrond"),
    RU("Фон"),
    TR("Arka plan"));

SS_MSG(timing_part_regularizer,
    EN("Regularizers"),
    JA("正則化"),
    ZH_HANS("正则化"),
    ZH_HANT("正則化"),
    KO("정규화"),
    DE("Regularisierung"),
    FR("Régularisations"),
    ES("Regularizadores"),
    PT("Regularizadores"),
    IT("Regolarizzatori"),
    NL("Regularisatie"),
    RU("Регуляризаторы"),
    TR("Düzenlileştirme"));

SS_MSG(timing_part_unclassified,
    EN("Unclassified kernels"),
    JA("未分類のカーネル"),
    ZH_HANS("未分类的内核"),
    ZH_HANT("未分類的核心"),
    KO("분류되지 않은 커널"),
    DE("Nicht eingeordnete Kernels"),
    FR("Noyaux non classés"),
    ES("Kernels sin clasificar"),
    PT("Kernels não classificados"),
    IT("Kernel non classificati"),
    NL("Niet-ingedeelde kernels"),
    RU("Неклассифицированные ядра"),
    TR("Sınıflandırılmamış çekirdekler"));

SS_MSG(timing_part_loading,
    EN("Image loading"),
    JA("画像の読み込み"),
    ZH_HANS("图像加载"),
    ZH_HANT("影像載入"),
    KO("이미지 불러오기"),
    DE("Bilder laden"),
    FR("Chargement des images"),
    ES("Carga de imágenes"),
    PT("Carregamento de imagens"),
    IT("Caricamento immagini"),
    NL("Beelden laden"),
    RU("Загрузка изображений"),
    TR("Görüntü yükleme"));

SS_MSG(timing_part_saving,
    EN("Checkpoints"),
    JA("チェックポイント"),
    ZH_HANS("检查点"),
    ZH_HANT("檢查點"),
    KO("체크포인트"),
    DE("Checkpoints"),
    FR("Points de sauvegarde"),
    ES("Puntos de control"),
    PT("Checkpoints"),
    IT("Checkpoint"),
    NL("Checkpoints"),
    RU("Контрольные точки"),
    TR("Kontrol noktaları"));

SS_MSG(timing_unaccounted,
    EN("Unaccounted"),
    JA("未計上"),
    ZH_HANS("未计入"),
    ZH_HANT("未計入"),
    KO("미집계"),
    DE("Nicht zugeordnet"),
    FR("Non attribué"),
    ES("Sin asignar"),
    PT("Não atribuído"),
    IT("Non attribuito"),
    NL("Niet toegewezen"),
    RU("Неучтённое"),
    TR("Hesaba katılmayan"));

SS_MSG(timing_scale_splats_pixels,
    EN("Splats × pixels"),
    JA("スプラット × 画素"),
    ZH_HANS("泼溅 × 像素"),
    ZH_HANT("潑濺 × 像素"),
    KO("스플랫 × 픽셀"),
    DE("Splats × Pixel"),
    FR("Splats × pixels"),
    ES("Splats × píxeles"),
    PT("Splats × pixels"),
    IT("Splat × pixel"),
    NL("Splats × pixels"),
    RU("Сплаты × пиксели"),
    TR("Splat × piksel"));

SS_MSG(timing_scale_pixels,
    EN("Pixels"),
    JA("画素"),
    ZH_HANS("像素"),
    ZH_HANT("像素"),
    KO("픽셀"),
    DE("Pixel"),
    FR("Pixels"),
    ES("Píxeles"),
    PT("Pixels"),
    IT("Pixel"),
    NL("Pixels"),
    RU("Пиксели"),
    TR("Pikseller"));

SS_MSG(timing_scale_images,
    EN("Training images"),
    JA("学習画像の数"),
    ZH_HANS("训练图像数"),
    ZH_HANT("訓練影像數"),
    KO("학습 이미지 수"),
    DE("Trainingsbilder"),
    FR("Images d'entraînement"),
    ES("Imágenes de entrenamiento"),
    PT("Imagens de treino"),
    IT("Immagini di addestramento"),
    NL("Trainingsbeelden"),
    RU("Обучающие изображения"),
    TR("Eğitim görüntüleri"));

SS_MSG(timing_scale_fixed,
    EN("Fixed"),
    JA("一定"),
    ZH_HANS("固定"),
    ZH_HANT("固定"),
    KO("고정"),
    DE("Fest"),
    FR("Fixe"),
    ES("Fijo"),
    PT("Fixo"),
    IT("Fisso"),
    NL("Vast"),
    RU("Постоянное"),
    TR("Sabit"));

SS_MSG(timing_tips_title,
    EN("Biggest savings"),
    JA("大きく短縮できる方法"),
    ZH_HANS("最能省时的方法"),
    ZH_HANT("最能省時的方法"),
    KO("가장 크게 줄이는 방법"),
    DE("Größte Einsparungen"),
    FR("Plus gros gains"),
    ES("Mayores ahorros"),
    PT("Maiores economias"),
    IT("Risparmi maggiori"),
    NL("Grootste besparingen"),
    RU("Наибольшая экономия"),
    TR("En büyük kazançlar"));

SS_MSG(timing_tip_halve,
    EN("Halve “{1}” in {2}: about {0}% less time"),
    JA("{2}の「{1}」を半分に: 約 {0}% 短縮"),
    ZH_HANS("将{2}中的“{1}”减半：约少用 {0}% 时间"),
    ZH_HANT("將{2}中的「{1}」減半：約少用 {0}% 時間"),
    KO("{2}의 '{1}'을(를) 절반으로: 시간 약 {0}% 단축"),
    DE("„{1}“ in {2} halbieren: etwa {0} % weniger Zeit"),
    FR("Diviser par deux « {1} » dans {2} : environ {0} % de temps en moins"),
    ES("Reducir a la mitad «{1}» en {2}: un {0} % menos de tiempo aprox."),
    PT("Reduzir pela metade “{1}” em {2}: cerca de {0}% menos tempo"),
    IT("Dimezzare «{1}» in {2}: circa il {0}% di tempo in meno"),
    NL("„{1}” halveren in {2}: ongeveer {0}% minder tijd"),
    RU("Уменьшить вдвое «{1}» в разделе «{2}»: примерно на {0}% меньше времени"),
    TR("{2} içinde “{1}” değerini yarıya indirin: yaklaşık %{0} daha az süre"));

SS_MSG(timing_tip_off,
    EN("Turn off “{1}” in {2}: about {0}% less time"),
    JA("{2}の「{1}」をオフに: 約 {0}% 短縮"),
    ZH_HANS("关闭{2}中的“{1}”：约少用 {0}% 时间"),
    ZH_HANT("關閉{2}中的「{1}」：約少用 {0}% 時間"),
    KO("{2}의 '{1}' 끄기: 시간 약 {0}% 단축"),
    DE("„{1}“ in {2} ausschalten: etwa {0} % weniger Zeit"),
    FR("Désactiver « {1} » dans {2} : environ {0} % de temps en moins"),
    ES("Desactivar «{1}» en {2}: un {0} % menos de tiempo aprox."),
    PT("Desativar “{1}” em {2}: cerca de {0}% menos tempo"),
    IT("Disattivare «{1}» in {2}: circa il {0}% di tempo in meno"),
    NL("„{1}” uitzetten in {2}: ongeveer {0}% minder tijd"),
    RU("Отключить «{1}» в разделе «{2}»: примерно на {0}% меньше времени"),
    TR("{2} içinde “{1}” seçeneğini kapatın: yaklaşık %{0} daha az süre"));

SS_MSG(timing_tip_off_two,
    EN("Turn off “{1}” and “{2}” in {3}: about {0}% less time"),
    JA("{3}の「{1}」と「{2}」をオフに: 約 {0}% 短縮"),
    ZH_HANS("关闭{3}中的“{1}”和“{2}”：约少用 {0}% 时间"),
    ZH_HANT("關閉{3}中的「{1}」和「{2}」：約少用 {0}% 時間"),
    KO("{3}의 '{1}', '{2}' 끄기: 시간 약 {0}% 단축"),
    DE("„{1}“ und „{2}“ in {3} ausschalten: etwa {0} % weniger Zeit"),
    FR("Désactiver « {1} » et « {2} » dans {3} : environ {0} % de temps en moins"),
    ES("Desactivar «{1}» y «{2}» en {3}: un {0} % menos de tiempo aprox."),
    PT("Desativar “{1}” e “{2}” em {3}: cerca de {0}% menos tempo"),
    IT("Disattivare «{1}» e «{2}» in {3}: circa il {0}% di tempo in meno"),
    NL("„{1}” en „{2}” uitzetten in {3}: ongeveer {0}% minder tijd"),
    RU("Отключить «{1}» и «{2}» в разделе «{3}»: примерно на {0}% меньше времени"),
    TR("{3} içinde “{1}” ve “{2}” seçeneklerini kapatın: yaklaşık %{0} daha az süre"));

SS_MSG(timing_tip_set,
    EN("Set “{1}” to “{2}” in {3}: about {0}% less time"),
    JA("{3}の「{1}」を「{2}」に: 約 {0}% 短縮"),
    ZH_HANS("将{3}中的“{1}”设为“{2}”：约少用 {0}% 时间"),
    ZH_HANT("將{3}中的「{1}」設為「{2}」：約少用 {0}% 時間"),
    KO("{3}의 '{1}'을(를) '{2}'(으)로: 시간 약 {0}% 단축"),
    DE("„{1}“ in {3} auf „{2}“ stellen: etwa {0} % weniger Zeit"),
    FR("Régler « {1} » sur « {2} » dans {3} : environ {0} % de temps en moins"),
    ES("Poner «{1}» en «{2}» en {3}: un {0} % menos de tiempo aprox."),
    PT("Definir “{1}” como “{2}” em {3}: cerca de {0}% menos tempo"),
    IT("Impostare «{1}» su «{2}» in {3}: circa il {0}% di tempo in meno"),
    NL("„{1}” op „{2}” zetten in {3}: ongeveer {0}% minder tijd"),
    RU("Установить «{1}» в «{2}» в разделе «{3}»: примерно на {0}% меньше времени"),
    TR("{3} içinde “{1}” değerini “{2}” yapın: yaklaşık %{0} daha az süre"));

SS_MSG(timing_tip_raise,
    EN("Raise “{1}” in {2}: about {0}% less time"),
    JA("{2}の「{1}」を大きく: 約 {0}% 短縮"),
    ZH_HANS("调高{2}中的“{1}”：约少用 {0}% 时间"),
    ZH_HANT("調高{2}中的「{1}」：約少用 {0}% 時間"),
    KO("{2}의 '{1}' 늘리기: 시간 약 {0}% 단축"),
    DE("„{1}“ in {2} erhöhen: etwa {0} % weniger Zeit"),
    FR("Augmenter « {1} » dans {2} : environ {0} % de temps en moins"),
    ES("Subir «{1}» en {2}: un {0} % menos de tiempo aprox."),
    PT("Aumentar “{1}” em {2}: cerca de {0}% menos tempo"),
    IT("Aumentare «{1}» in {2}: circa il {0}% di tempo in meno"),
    NL("„{1}” verhogen in {2}: ongeveer {0}% minder tijd"),
    RU("Увеличить «{1}» в разделе «{2}»: примерно на {0}% меньше времени"),
    TR("{2} içinde “{1}” değerini artırın: yaklaşık %{0} daha az süre"));

SS_MSG(timing_tip_preset,
    EN("Change “{1}” from “{2}” to “{3}”: about {0}% less time"),
    JA("「{1}」を「{2}」から「{3}」に: 約 {0}% 短縮"),
    ZH_HANS("将“{1}”从“{2}”改为“{3}”：约少用 {0}% 时间"),
    ZH_HANT("將「{1}」從「{2}」改為「{3}」：約少用 {0}% 時間"),
    KO("'{1}'을(를) '{2}'에서 '{3}'(으)로: 시간 약 {0}% 단축"),
    DE("„{1}“ von „{2}“ auf „{3}“ ändern: etwa {0} % weniger Zeit"),
    FR("Passer « {1} » de « {2} » à « {3} » : environ {0} % de temps en moins"),
    ES("Cambiar «{1}» de «{2}» a «{3}»: un {0} % menos de tiempo aprox."),
    PT("Mudar “{1}” de “{2}” para “{3}”: cerca de {0}% menos tempo"),
    IT("Cambiare «{1}» da «{2}» a «{3}»: circa il {0}% di tempo in meno"),
    NL("„{1}” van „{2}” naar „{3}”: ongeveer {0}% minder tijd"),
    RU("Сменить «{1}» с «{2}» на «{3}»: примерно на {0}% меньше времени"),
    TR("“{1}” ayarını “{2}” yerine “{3}” yapın: yaklaşık %{0} daha az süre"));

SS_MSG(vram_tip_halve,
    EN("Halve “{1}” in {2}: about {0} GiB less"),
    JA("{2}の「{1}」を半分に: 約 {0} GiB 削減"),
    ZH_HANS("将{2}中的“{1}”减半：约少 {0} GiB"),
    ZH_HANT("將{2}中的「{1}」減半：約少 {0} GiB"),
    KO("{2}의 '{1}'을(를) 절반으로: 약 {0} GiB 절감"),
    DE("„{1}“ in {2} halbieren: etwa {0} GiB weniger"),
    FR("Diviser par deux « {1} » dans {2} : environ {0} Gio de moins"),
    ES("Reducir a la mitad «{1}» en {2}: unos {0} GiB menos"),
    PT("Reduzir pela metade “{1}” em {2}: cerca de {0} GiB a menos"),
    IT("Dimezzare «{1}» in {2}: circa {0} GiB in meno"),
    NL("„{1}” halveren in {2}: ongeveer {0} GiB minder"),
    RU("Уменьшить вдвое «{1}» в разделе «{2}»: примерно на {0} ГиБ меньше"),
    TR("{2} içinde “{1}” değerini yarıya indirin: yaklaşık {0} GiB daha az"));

SS_MSG(vram_tip_set,
    EN("Set “{1}” to “{2}” in {3}: about {0} GiB less"),
    JA("{3}の「{1}」を「{2}」に: 約 {0} GiB 削減"),
    ZH_HANS("将{3}中的“{1}”设为“{2}”：约少 {0} GiB"),
    ZH_HANT("將{3}中的「{1}」設為「{2}」：約少 {0} GiB"),
    KO("{3}의 '{1}'을(를) '{2}'(으)로: 약 {0} GiB 절감"),
    DE("„{1}“ in {3} auf „{2}“ stellen: etwa {0} GiB weniger"),
    FR("Régler « {1} » sur « {2} » dans {3} : environ {0} Gio de moins"),
    ES("Poner «{1}» en «{2}» en {3}: unos {0} GiB menos"),
    PT("Definir “{1}” como “{2}” em {3}: cerca de {0} GiB a menos"),
    IT("Impostare «{1}» su «{2}» in {3}: circa {0} GiB in meno"),
    NL("„{1}” op „{2}” zetten in {3}: ongeveer {0} GiB minder"),
    RU("Установить «{1}» в «{2}» в разделе «{3}»: примерно на {0} ГиБ меньше"),
    TR("{3} içinde “{1}” değerini “{2}” yapın: yaklaşık {0} GiB daha az"));

SS_MSG(vram_tip_preset,
    EN("Change “{1}” from “{2}” to “{3}”: about {0} GiB less"),
    JA("「{1}」を「{2}」から「{3}」に: 約 {0} GiB 削減"),
    ZH_HANS("将“{1}”从“{2}”改为“{3}”：约少 {0} GiB"),
    ZH_HANT("將「{1}」從「{2}」改為「{3}」：約少 {0} GiB"),
    KO("'{1}'을(를) '{2}'에서 '{3}'(으)로: 약 {0} GiB 절감"),
    DE("„{1}“ von „{2}“ auf „{3}“ ändern: etwa {0} GiB weniger"),
    FR("Passer « {1} » de « {2} » à « {3} » : environ {0} Gio de moins"),
    ES("Cambiar «{1}» de «{2}» a «{3}»: unos {0} GiB menos"),
    PT("Mudar “{1}” de “{2}” para “{3}”: cerca de {0} GiB a menos"),
    IT("Cambiare «{1}» da «{2}» a «{3}»: circa {0} GiB in meno"),
    NL("„{1}” van „{2}” naar „{3}”: ongeveer {0} GiB minder"),
    RU("Сменить «{1}» с «{2}» на «{3}»: примерно на {0} ГиБ меньше"),
    TR("“{1}” ayarını “{2}” yerine “{3}” yapın: yaklaşık {0} GiB daha az"));

SS_MSG(timing_tip_images,
    EN("Half as many training images: about {0}% less time"),
    JA("学習画像を半分に: 約 {0}% 短縮"),
    ZH_HANS("训练图像减半：约少用 {0}% 时间"),
    ZH_HANT("訓練影像減半：約少用 {0}% 時間"),
    KO("학습 이미지를 절반으로: 시간 약 {0}% 단축"),
    DE("Halb so viele Trainingsbilder: etwa {0} % weniger Zeit"),
    FR("Deux fois moins d'images d'entraînement : environ {0} % de temps en moins"),
    ES("La mitad de imágenes de entrenamiento: un {0} % menos de tiempo aprox."),
    PT("Metade das imagens de treino: cerca de {0}% menos tempo"),
    IT("Metà delle immagini di addestramento: circa il {0}% di tempo in meno"),
    NL("Half zoveel trainingsbeelden: ongeveer {0}% minder tijd"),
    RU("Вдвое меньше обучающих изображений: примерно на {0}% меньше времени"),
    TR("Yarı sayıda eğitim görüntüsü: yaklaşık %{0} daha az süre"));

SS_MSG(timing_tip_viewer,
    EN("Untick “{1}” above the viewport while training: about {0}% less time"),
    JA("学習中はビューポート上部の「{1}」をオフに: 約 {0}% 短縮"),
    ZH_HANS("训练时取消勾选视口上方的“{1}”：约少用 {0}% 时间"),
    ZH_HANT("訓練時取消勾選視窗上方的「{1}」：約少用 {0}% 時間"),
    KO("학습 중 뷰포트 위의 '{1}' 해제: 시간 약 {0}% 단축"),
    DE("Beim Training „{1}“ über der Ansicht abwählen: etwa {0} % weniger Zeit"),
    FR("Décocher « {1} » au-dessus de la vue pendant l'entraînement : environ {0} % de temps en moins"),
    ES("Desmarcar «{1}» sobre la vista durante el entrenamiento: un {0} % menos de tiempo aprox."),
    PT("Desmarcar “{1}” acima da visualização durante o treino: cerca de {0}% menos tempo"),
    IT("Deselezionare «{1}» sopra la vista durante l'addestramento: circa il {0}% di tempo in meno"),
    NL("„{1}” boven de weergave uitvinken tijdens het trainen: ongeveer {0}% minder tijd"),
    RU("Снять «{1}» над окном просмотра во время обучения: примерно на {0}% меньше времени"),
    TR("Eğitim sırasında görüntü alanının üstündeki “{1}” işaretini kaldırın: yaklaşık %{0} daha az süre"));

SS_MSG(timing_tip_loading,
    EN("Waiting for images: {0}% of the time. Keeping the dataset on a faster disk helps."),
    JA("画像の待ち時間が {0}%。データセットを高速なディスクに置くと改善します。"),
    ZH_HANS("等待图像占 {0}% 时间。把数据集放在更快的磁盘上会有帮助。"),
    ZH_HANT("等待影像占 {0}% 時間。把資料集放在更快的磁碟上會有幫助。"),
    KO("이미지 대기가 시간의 {0}%. 데이터셋을 더 빠른 디스크에 두면 도움이 됩니다."),
    DE("Warten auf Bilder: {0} % der Zeit. Den Datensatz auf eine schnellere Platte zu legen hilft."),
    FR("Attente des images : {0} % du temps. Placer le jeu de données sur un disque plus rapide aide."),
    ES("Espera de imágenes: {0} % del tiempo. Ayuda poner el conjunto de datos en un disco más rápido."),
    PT("Espera por imagens: {0}% do tempo. Colocar o conjunto de dados num disco mais rápido ajuda."),
    IT("Attesa delle immagini: {0}% del tempo. Aiuta tenere il dataset su un disco più veloce."),
    NL("Wachten op beelden: {0}% van de tijd. De dataset op een snellere schijf zetten helpt."),
    RU("Ожидание изображений: {0}% времени. Поможет более быстрый диск для набора данных."),
    TR("Görüntü bekleme: sürenin %{0}'i. Veri kümesini daha hızlı bir diske koymak yardımcı olur."));

SS_MSG(vram_tips_title,
    EN("Biggest savings at the peak"),
    JA("ピーク時に大きく減らせる方法"),
    ZH_HANS("峰值时最能省显存的方法"),
    ZH_HANT("峰值時最能省顯示記憶體的方法"),
    KO("최대치에서 가장 크게 줄이는 방법"),
    DE("Größte Einsparungen an der Spitze"),
    FR("Plus gros gains au pic"),
    ES("Mayores ahorros en el pico"),
    PT("Maiores economias no pico"),
    IT("Risparmi maggiori al picco"),
    NL("Grootste besparingen op de piek"),
    RU("Наибольшая экономия на пике"),
    TR("Tepede en büyük kazançlar"));

// ===========================================================================
// Log panel
// ===========================================================================

SS_MSG(log_details,
    EN("Show Every Line"),
    JA("すべての行を表示"),
    ZH_HANS("显示每一行"),
    ZH_HANT("顯示每一行"),
    KO("모든 줄 보기"),
    DE("Jede Zeile anzeigen"),
    FR("Afficher toutes les lignes"),
    ES("Mostrar todas las líneas"),
    PT("Mostrar todas as linhas"),
    IT("Mostra tutte le righe"),
    NL("Elke regel tonen"),
    RU("Показывать все строки"),
    TR("Her satırı göster"));

SS_MSG(log_follow,
    EN("Follow New Output"),
    JA("新しい出力を追いかける"),
    ZH_HANS("跟随新输出"),
    ZH_HANT("跟隨新輸出"),
    KO("새 출력 따라가기"),
    DE("Neuer Ausgabe folgen"),
    FR("Suivre les nouvelles lignes"),
    ES("Seguir la salida nueva"),
    PT("Acompanhar a saída nova"),
    IT("Segui il nuovo output"),
    NL("Nieuwe uitvoer volgen"),
    RU("Следить за новым выводом"),
    TR("Yeni çıktıyı izle"));

SS_MSG(log_jump,
    EN("Jump to latest"),
    JA("最新へ移動"),
    ZH_HANS("跳到最新"),
    ZH_HANT("跳到最新"),
    KO("최신으로 이동"),
    DE("Zum Neuesten springen"),
    FR("Aller au plus récent"),
    ES("Ir a lo más reciente"),
    PT("Ir para o mais recente"),
    IT("Vai al più recente"),
    NL("Naar het nieuwste"),
    RU("Перейти к последнему"),
    TR("En sona git"));

SS_MSG(log_copy,
    EN("Copy All"),
    JA("すべてコピー"),
    ZH_HANS("全部复制"),
    ZH_HANT("全部複製"),
    KO("모두 복사"),
    DE("Alles kopieren"),
    FR("Tout copier"),
    ES("Copiar todo"),
    PT("Copiar tudo"),
    IT("Copia tutto"),
    NL("Alles kopiëren"),
    RU("Копировать всё"),
    TR("Tümünü kopyala"));

SS_MSG(log_clear,
    EN("Clear"),
    JA("消去"),
    ZH_HANS("清空"),
    ZH_HANT("清空"),
    KO("지우기"),
    DE("Leeren"),
    FR("Effacer"),
    ES("Vaciar"),
    PT("Limpar"),
    IT("Svuota"),
    NL("Wissen"),
    RU("Очистить"),
    TR("Temizle"));


// ===========================================================================
// Stop-training confirmation
// ===========================================================================
// Three whole sentences rather than one with a swappable tail: "Stop training
// and {0}?" cannot be translated into a verb-final language without knowing
// what {0} is.

SS_MSG(confirm_title,
    EN("Stop training?"), JA("学習を停止しますか？"), ZH_HANS("要停止训练吗？"),
    ZH_HANT("要停止訓練嗎？"), KO("학습을 멈출까요?"), DE("Training anhalten?"),
    FR("Arrêter l'entraînement ?"), ES("¿Detener el entrenamiento?"),
    PT("Parar o treinamento?"), IT("Fermare l'addestramento?"),
    NL("Training stoppen?"), RU("Остановить обучение?"), TR("Eğitim durdurulsun mu?"));

SS_MSG(confirm_intro,
    EN("Training is paused while you decide."),
    JA("決めるあいだ、学習は一時停止しています。"),
    ZH_HANS("训练已暂停，等待你的选择。"),
    ZH_HANT("訓練已暫停，等待你的選擇。"),
    KO("결정하는 동안 학습을 멈춰 두었습니다."),
    DE("Das Training ist angehalten, solange Sie entscheiden."),
    FR("L'entraînement est en pause le temps de votre choix."),
    ES("El entrenamiento está en pausa mientras decide."),
    PT("O treinamento está pausado enquanto você decide."),
    IT("L'addestramento è in pausa mentre decide."),
    NL("De training staat stil terwijl u kiest."),
    RU("Обучение приостановлено, пока вы выбираете."),
    TR("Siz karar verirken eğitim duraklatıldı."));

SS_MSG(confirm_quit,
    EN("Stop training and exit?"),
    JA("学習を停止して終了しますか？"),
    ZH_HANS("停止训练并退出吗？"),
    ZH_HANT("停止訓練並結束嗎？"),
    KO("학습을 멈추고 종료할까요?"),
    DE("Training anhalten und beenden?"),
    FR("Arrêter l'entraînement et quitter ?"),
    ES("¿Detener el entrenamiento y salir?"),
    PT("Parar o treinamento e sair?"),
    IT("Fermare l'addestramento e uscire?"),
    NL("Training stoppen en afsluiten?"),
    RU("Остановить обучение и выйти?"),
    TR("Eğitimi durdurup çıkalım mı?"));

SS_MSG(confirm_home,
    EN("Stop training and go to the home screen?"),
    JA("学習を停止してホーム画面に戻りますか？"),
    ZH_HANS("停止训练并回到主页吗？"),
    ZH_HANT("停止訓練並回到首頁嗎？"),
    KO("학습을 멈추고 홈 화면으로 갈까요?"),
    DE("Training anhalten und zum Startbildschirm gehen?"),
    FR("Arrêter l'entraînement et revenir à l'accueil ?"),
    ES("¿Detener el entrenamiento e ir a la pantalla de inicio?"),
    PT("Parar o treinamento e ir para a tela inicial?"),
    IT("Fermare l'addestramento e tornare alla schermata iniziale?"),
    NL("Training stoppen en naar het beginscherm gaan?"),
    RU("Остановить обучение и вернуться на главный экран?"),
    TR("Eğitimi durdurup ana ekrana dönelim mi?"));

SS_MSG(confirm_open,
    EN("Stop training and open the new dataset?"),
    JA("学習を停止して新しいデータセットを開きますか？"),
    ZH_HANS("停止训练并打开新的数据集吗？"),
    ZH_HANT("停止訓練並開啟新的資料集嗎？"),
    KO("학습을 멈추고 새 데이터셋을 열까요?"),
    DE("Training anhalten und den neuen Datensatz öffnen?"),
    FR("Arrêter l'entraînement et ouvrir le nouveau jeu de données ?"),
    ES("¿Detener el entrenamiento y abrir el nuevo conjunto de datos?"),
    PT("Parar o treinamento e abrir o novo conjunto de dados?"),
    IT("Fermare l'addestramento e aprire il nuovo set di dati?"),
    NL("Training stoppen en de nieuwe dataset openen?"),
    RU("Остановить обучение и открыть новый набор данных?"),
    TR("Eğitimi durdurup yeni veri kümesini açalım mı?"));

SS_MSG(keep_training,
    EN("Keep Training"), JA("学習を続ける"),   ZH_HANS("继续训练"),  ZH_HANT("繼續訓練"),
    KO("계속 학습"),      DE("Weiter trainieren"), FR("Continuer l'entraînement"),
    ES("Seguir entrenando"), PT("Continuar treinando"),
    IT("Continua l'addestramento"), NL("Doorgaan met trainen"),
    RU("Продолжить обучение"), TR("Eğitime devam et"));

SS_MSG(keep_training_help,
    EN("Close this and carry on from where the run was paused."),
    JA("これを閉じて、一時停止したところから続けます。"),
    ZH_HANS("关掉这个对话框，从暂停的地方继续训练。"),
    ZH_HANT("關掉這個對話框，從暫停的地方繼續訓練。"),
    KO("이 창을 닫고 멈춘 자리에서 이어서 학습합니다."),
    DE("Schließt dies und macht dort weiter, wo der Lauf angehalten wurde."),
    FR("Ferme cette fenêtre et reprend là où l'exécution a été mise en pause."),
    ES("Cierra esto y sigue desde donde se pausó la ejecución."),
    PT("Fecha isto e continua de onde a execução foi pausada."),
    IT("Chiude questa finestra e riprende da dove l'esecuzione è stata messa "
       "in pausa."),
    NL("Sluit dit en gaat verder waar de run is stilgezet."),
    RU("Закрывает это окно и продолжает с места, где прогон был приостановлен."),
    TR("Bunu kapatır ve çalışmanın duraklatıldığı yerden devam eder."));

SS_MSG(data_error_title,
    EN("Training paused"),
    JA("学習を一時停止しました"),
    ZH_HANS("训练已暂停"),
    ZH_HANT("訓練已暫停"),
    KO("학습이 일시정지됨"),
    DE("Training pausiert"),
    FR("Entraînement en pause"),
    ES("Entrenamiento en pausa"),
    PT("Treinamento pausado"),
    IT("Addestramento in pausa"),
    NL("Training gepauzeerd"),
    RU("Обучение приостановлено"),
    TR("Eğitim duraklatıldı"));

SS_MSG(data_error_intro,
    EN("Training is paused here and nothing is lost. Clear the problem above, "
       "then retry."),
    JA("ここで学習は一時停止していて、失われたものはありません。上の問題を"
       "解消してから、やり直してください。"),
    ZH_HANS("训练已在此暂停，没有任何损失。解决上面的问题，然后重试。"),
    ZH_HANT("訓練已在此暫停，沒有任何損失。解決上面的問題，然後重試。"),
    KO("여기서 학습이 멈춰 있고 잃은 것은 없습니다. 위의 문제를 해결한 뒤 다시 "
       "시도하세요."),
    DE("Das Training pausiert hier, nichts geht verloren. Beheben Sie das "
       "Problem oben und versuchen Sie es erneut."),
    FR("L'entraînement est en pause ici et rien n'est perdu. Corrigez le "
       "problème ci-dessus, puis réessayez."),
    ES("El entrenamiento está en pausa aquí y no se pierde nada. Resuelve el "
       "problema de arriba y reinténtalo."),
    PT("O treinamento está pausado aqui e nada se perde. Resolva o problema "
       "acima e tente novamente."),
    IT("L'addestramento è in pausa qui e non si perde nulla. Risolvi il "
       "problema qui sopra, poi riprova."),
    NL("De training staat hier stil en er gaat niets verloren. Los het "
       "probleem hierboven op en probeer het opnieuw."),
    RU("Обучение приостановлено здесь, ничего не потеряно. Устраните проблему "
       "выше и повторите."),
    TR("Eğitim burada duraklatıldı ve hiçbir şey kaybolmadı. Yukarıdaki "
       "sorunu giderip yeniden deneyin."));

SS_MSG(data_error_retry,
    EN("Retry"), JA("やり直す"), ZH_HANS("重试"), ZH_HANT("重試"),
    KO("다시 시도"), DE("Erneut versuchen"), FR("Réessayer"),
    ES("Reintentar"), PT("Tentar de novo"), IT("Riprova"),
    NL("Opnieuw proberen"), RU("Повторить"), TR("Yeniden dene"));

SS_MSG(data_error_retry_help,
    EN("Read the file again and carry on from this step."),
    JA("ファイルを読み直して、このステップから続けます。"),
    ZH_HANS("重新读取该文件，并从这一步继续。"),
    ZH_HANT("重新讀取該檔案，並從這一步繼續。"),
    KO("파일을 다시 읽고 이 단계부터 이어서 진행합니다."),
    DE("Liest die Datei erneut und macht ab diesem Schritt weiter."),
    FR("Relit le fichier et reprend à partir de cette étape."),
    ES("Vuelve a leer el archivo y sigue desde este paso."),
    PT("Lê o ficheiro de novo e continua a partir deste passo."),
    IT("Rilegge il file e riprende da questo passo."),
    NL("Leest het bestand opnieuw en gaat verder vanaf deze stap."),
    RU("Читает файл заново и продолжает с этого шага."),
    TR("Dosyayı yeniden okur ve bu adımdan devam eder."));

SS_MSG(confirm_stop,
    EN("Stop training and keep the result loaded for viewing?"),
    JA("学習を停止し、結果は表示用に読み込んだままにしますか？"),
    ZH_HANS("停止训练，并把结果留在内存中以便查看吗？"),
    ZH_HANT("停止訓練，並把結果留在記憶體中以便檢視嗎？"),
    KO("학습을 멈추고 결과는 볼 수 있도록 그대로 둘까요?"),
    DE("Training anhalten und das Ergebnis zum Betrachten geladen lassen?"),
    FR("Arrêter l'entraînement et garder le résultat chargé pour le consulter ?"),
    ES("¿Detener el entrenamiento y dejar el resultado cargado para verlo?"),
    PT("Parar o treinamento e deixar o resultado carregado para visualização?"),
    IT("Fermare l'addestramento e lasciare il risultato caricato per guardarlo?"),
    NL("Training stoppen en het resultaat geladen laten om te bekijken?"),
    RU("Остановить обучение и оставить результат загруженным для просмотра?"),
    TR("Eğitimi durdurup sonucu görüntülemek için yüklü bırakalım mı?"));

// ===========================================================================
// Viewport
// ===========================================================================

SS_MSG(viewport_dataset_preview,
    EN("dataset preview"), JA("データセットのプレビュー"), ZH_HANS("数据集预览"),
    ZH_HANT("資料集預覽"), KO("데이터셋 미리보기"), DE("Datensatzvorschau"),
    FR("aperçu du jeu de données"), ES("vista previa del conjunto"),
    PT("prévia do conjunto"), IT("anteprima del set di dati"),
    NL("datasetvoorbeeld"), RU("предпросмотр набора"), TR("veri kümesi önizlemesi"));

SS_MSG(viewport_dataset_preview_help,
    EN("Sparse SfM point cloud and camera poses of the loaded dataset. "
       "Training replaces this with the live splat render."),
    JA("読み込んだデータセットの疎な SfM 点群とカメラ姿勢です。学習を始めると、"
       "スプラットのライブ描画に切り替わります。"),
    ZH_HANS("已加载数据集的稀疏 SfM 点云和相机位姿。训练开始后会换成泼溅的实时渲染。"),
    ZH_HANT("已載入資料集的稀疏 SfM 點雲和相機姿態。訓練開始後會換成潑濺的即時算繪。"),
    KO("불러온 데이터셋의 희소 SfM 점 구름과 카메라 자세입니다. 학습이 시작되면 "
       "스플랫 실시간 렌더링으로 바뀝니다."),
    DE("Dünne SfM-Punktwolke und Kameraposen des geladenen Datensatzes. Beim "
       "Training tritt an ihre Stelle die laufende Splat-Darstellung."),
    FR("Nuage de points SfM épars et poses de caméra du jeu de données "
       "chargé. L'entraînement remplace cela par le rendu de splats en direct."),
    ES("Nube de puntos SfM dispersa y poses de cámara del conjunto cargado. "
       "Al entrenar, esto se sustituye por el render de splats en vivo."),
    PT("Nuvem de pontos SfM esparsa e poses de câmera do conjunto carregado. "
       "O treinamento substitui isso pela renderização de splats ao vivo."),
    IT("Nuvola di punti SfM sparsa e pose della fotocamera del set caricato. "
       "L'addestramento la sostituisce con il rendering dal vivo degli splat."),
    NL("IJle SfM-puntenwolk en cameraposities van de geladen dataset. Bij "
       "training komt hier de live splat-weergave voor in de plaats."),
    RU("Разрежённое облако точек SfM и позы камер загруженного набора. При "
       "обучении вместо него показывается живая отрисовка сплатов."),
    TR("Yüklenen veri kümesinin seyrek SfM nokta bulutu ve kamera duruşları. "
       "Eğitim başlayınca yerini canlı splat işlemesi alır."));

SS_MSG(viewport_buffer_help,
    EN("Which render buffer to display (color, depth, alpha, normals from "
       "depth, ...)."),
    JA("表示するレンダーバッファを選びます（カラー、深度、アルファ、深度から"
       "求めた法線など）。"),
    ZH_HANS("显示哪个渲染缓冲（颜色、深度、alpha、由深度推出的法线等）。"),
    ZH_HANT("顯示哪個算繪緩衝（顏色、深度、alpha、由深度推出的法線等）。"),
    KO("어떤 렌더 버퍼를 보여줄지 고릅니다(색, 깊이, 알파, 깊이에서 구한 법선 등)."),
    DE("Welcher Renderpuffer angezeigt wird (Farbe, Tiefe, Alpha, aus der "
       "Tiefe berechnete Normalen …)."),
    FR("Quel tampon de rendu afficher (couleur, profondeur, alpha, normales "
       "issues de la profondeur…)."),
    ES("Qué búfer de render mostrar (color, profundidad, alfa, normales a "
       "partir de la profundidad…)."),
    PT("Qual buffer de renderização mostrar (cor, profundidade, alfa, normais "
       "a partir da profundidade…)."),
    IT("Quale buffer di rendering mostrare (colore, profondità, alfa, normali "
       "ricavate dalla profondità…)."),
    NL("Welke renderbuffer wordt getoond (kleur, diepte, alfa, normalen uit "
       "diepte…)."),
    RU("Какой буфер отрисовки показывать (цвет, глубина, альфа, нормали из "
       "глубины…)."),
    TR("Hangi işleme arabelleğinin gösterileceği (renk, derinlik, alfa, "
       "derinlikten normaller…)."));

SS_MSG(viewport_cameras,
    EN("cameras"),       JA("カメラ"),        ZH_HANS("相机"),     ZH_HANT("相機"),
    KO("카메라"),         DE("Kameras"),      FR("caméras"),      ES("cámaras"),
    PT("câmeras"),       IT("fotocamere"),   NL("camera's"),     RU("камеры"),
    TR("kameralar"));

SS_MSG(viewport_cameras_help,
    EN("Overlay the training camera frusta (during training, with image "
       "thumbnails once visited)."),
    JA("学習用カメラの視錐台を重ねて表示します（学習中は、一度使われた"
       "カメラにサムネイルが付きます）。"),
    ZH_HANS("叠加显示训练相机的视锥（训练时，用过的相机会带上缩略图）。"),
    ZH_HANT("疊加顯示訓練相機的視錐（訓練時，用過的相機會帶上縮圖）。"),
    KO("학습 카메라의 절두체를 겹쳐 보여줍니다(학습 중에는 한 번 쓰인 카메라에 "
       "썸네일이 붙습니다)."),
    DE("Die Sichtkegel der Trainingskameras einblenden (während des Trainings "
       "mit Miniaturbildern, sobald eine Kamera an der Reihe war)."),
    FR("Superposer les frustums des caméras d'entraînement (pendant "
       "l'entraînement, avec une vignette une fois la caméra visitée)."),
    ES("Superponer los frustums de las cámaras de entrenamiento (durante el "
       "entrenamiento, con miniatura una vez visitadas)."),
    PT("Sobrepor os frustums das câmeras de treinamento (durante o "
       "treinamento, com miniatura assim que visitadas)."),
    IT("Sovrapporre i frustum delle fotocamere di addestramento (durante "
       "l'addestramento, con miniatura una volta visitate)."),
    NL("De frusta van de trainingscamera's overlappen (tijdens de training "
       "met miniaturen zodra ze aan de beurt zijn geweest)."),
    RU("Показывать поверх пирамиды видимости обучающих камер (во время "
       "обучения — с миниатюрами у уже использованных)."),
    TR("Eğitim kameralarının görüş piramitlerini üstüne bindirir (eğitim "
       "sırasında, sırası gelmiş kameralara küçük resim eklenir)."));

SS_MSG(viewport_frustum_size_help,
    EN("Camera frustum display size."),
    JA("カメラ視錐台の表示サイズです。"),
    ZH_HANS("相机视锥的显示大小。"),
    ZH_HANT("相機視錐的顯示大小。"),
    KO("카메라 절두체의 표시 크기입니다."),
    DE("Anzeigegröße der Kamerasichtkegel."),
    FR("Taille d'affichage des frustums de caméra."),
    ES("Tamaño con que se dibujan los frustums de cámara."),
    PT("Tamanho de exibição dos frustums de câmera."),
    IT("Dimensione con cui vengono disegnati i frustum."),
    NL("Weergavegrootte van de camerafrusta."),
    RU("Размер отображения пирамид видимости камер."),
    TR("Kamera görüş piramitlerinin görüntülenme boyutu."));

SS_MSG(viewport_point_size,
    EN("Points: {0} px"),
    JA("点: {0} px"),
    ZH_HANS("点：{0} px"),
    ZH_HANT("點：{0} px"),
    KO("점: {0} px"),
    DE("Punkte: {0} px"),
    FR("Points : {0} px"),
    ES("Puntos: {0} px"),
    PT("Pontos: {0} px"),
    IT("Punti: {0} px"),
    NL("Punten: {0} px"),
    RU("Точки: {0} px"),
    TR("Noktalar: {0} px"));

SS_MSG(viewport_point_size_help,
    EN("How many pixels across each point of the reconstruction is drawn."),
    JA("再構成の点を何ピクセルの幅で描くかです。"),
    ZH_HANS("重建中每个点绘制的像素宽度。"),
    ZH_HANT("重建中每個點繪製的像素寬度。"),
    KO("재구성의 각 점을 몇 픽셀 폭으로 그릴지입니다."),
    DE("Wie viele Pixel breit jeder Punkt der Rekonstruktion gezeichnet wird."),
    FR("Largeur en pixels de chaque point de la reconstruction à l'écran."),
    ES("Cuántos píxeles de ancho mide cada punto de la reconstrucción en pantalla."),
    PT("Quantos pixels de largura tem cada ponto da reconstrução na tela."),
    IT("Quanti pixel di larghezza ha ogni punto della ricostruzione a schermo."),
    NL("Hoeveel pixels breed elk punt van de reconstructie getekend wordt."),
    RU("Ширина каждой точки реконструкции на экране в пикселях."),
    TR("Rekonstrüksiyonun her noktasının kaç piksel genişliğinde çizileceği."));

SS_MSG(viewport_grid,
    EN("grid"),          JA("グリッド"),      ZH_HANS("网格"),     ZH_HANT("格線"),
    KO("격자"),           DE("Raster"),       FR("grille"),       ES("rejilla"),
    PT("grade"),         IT("griglia"),      NL("raster"),       RU("сетка"),
    TR("ızgara"));

SS_MSG(viewport_grid_help,
    EN("Overlay a ground-plane grid and the X, Y and Z axes. Its cells are a "
       "power of ten that follows the zoom; the corner of the view shows one "
       "cell's size."),
    JA("地面のグリッドと X・Y・Z 軸を重ねて表示します。マス目は 10 のべき乗の"
       "大きさでズームに合わせて変わり、1 マスの大きさはビューの隅に表示されます。"),
    ZH_HANS("叠加显示地面网格和 X、Y、Z 轴。格子大小是 10 的幂，随缩放变化；"
            "视图角落显示一格的大小。"),
    ZH_HANT("疊加顯示地面格線和 X、Y、Z 軸。格子大小是 10 的冪，隨縮放變化；"
            "視圖角落顯示一格的大小。"),
    KO("지면 격자와 X·Y·Z 축을 겹쳐 보여줍니다. 칸 크기는 확대·축소에 따라 "
       "바뀌는 10의 거듭제곱이며, 한 칸의 크기는 뷰 모서리에 표시됩니다."),
    DE("Ein Bodenraster und die X-, Y- und Z-Achsen einblenden. Die Zellen sind "
       "eine Zehnerpotenz, die dem Zoom folgt; die Ecke der Ansicht zeigt die "
       "Größe einer Zelle."),
    FR("Superposer une grille au sol et les axes X, Y et Z. Ses cellules sont "
       "une puissance de dix qui suit le zoom ; le coin de la vue indique la "
       "taille d'une cellule."),
    ES("Superponer una rejilla en el suelo y los ejes X, Y y Z. Sus celdas son "
       "una potencia de diez que sigue al zoom; la esquina de la vista indica "
       "el tamaño de una celda."),
    PT("Sobrepor uma grade no chão e os eixos X, Y e Z. As células são uma "
       "potência de dez que acompanha o zoom; o canto da vista mostra o "
       "tamanho de uma célula."),
    IT("Sovrapporre una griglia a terra e gli assi X, Y e Z. Le celle sono una "
       "potenza di dieci che segue lo zoom; l'angolo della vista mostra la "
       "dimensione di una cella."),
    NL("Een grondraster en de X-, Y- en Z-as tonen. De cellen zijn een macht "
       "van tien die de zoom volgt; de hoek van het beeld toont de grootte van "
       "één cel."),
    RU("Показывать поверх сетку на плоскости земли и оси X, Y и Z. Размер "
       "клетки — степень десяти, меняется вместе с масштабом; размер одной "
       "клетки указан в углу вида."),
    TR("Zemin ızgarasını ve X, Y, Z eksenlerini üstüne bindirir. Hücreler "
       "yakınlaştırmaya göre değişen bir on kuvvetidir; bir hücrenin boyutu "
       "görünümün köşesinde yazar."));

SS_MSG(viewport_region,
    EN("region"),        JA("領域"),          ZH_HANS("区域"),     ZH_HANT("區域"),
    KO("영역"),           DE("Bereich"),      FR("région"),       ES("región"),
    PT("região"),        IT("regione"),      NL("gebied"),       RU("область"),
    TR("bölge"));

SS_MSG(viewport_region_help,
    EN("The region of interest this run trains, drawn as a tinted surface with a dashed outline; fainter where the scene is in front of it. Splats outside it are rarely densified."),
    JA("この実行が学習する注目領域。色付きの面と破線の輪郭で描き、シーンの陰になる部分は薄く表示します。領域外のスプラットはほとんど高密度化されません。"),
    ZH_HANS("本次训练的感兴趣区域，以着色表面和虚线轮廓显示；被场景遮挡的部分较淡。区域外的高斯点很少被加密。"),
    ZH_HANT("本次訓練的感興趣區域，以著色表面和虛線輪廓顯示；被場景遮擋的部分較淡。區域外的高斯點很少被加密。"),
    KO("이 실행이 학습하는 관심 영역을 색칠된 면과 점선 윤곽으로 표시합니다. 장면에 가려진 부분은 흐리게 보입니다. 영역 밖의 스플랫은 거의 조밀화되지 않습니다."),
    DE("Der Interessenbereich, den dieser Lauf trainiert, als getönte Fläche mit gestrichelter Kontur; blasser, wo die Szene davor liegt. Splats außerhalb werden kaum verdichtet."),
    FR("La région d'intérêt que cet entraînement apprend, en surface teintée au contour pointillé ;\nplus pâle là où la scène passe devant. Les splats hors de la région sont rarement densifiés."),
    ES("La región de interés que entrena esta ejecución, como superficie tintada con contorno discontinuo;\nmás tenue donde la escena queda delante. Los splats fuera de ella apenas se densifican."),
    PT("A região de interesse que esta execução treina, como superfície tingida com contorno tracejado;\nmais clara onde a cena fica à frente. Splats fora dela quase não são densificados."),
    IT("La regione di interesse che questa esecuzione addestra, come superficie colorata con contorno tratteggiato;\npiù tenue dove la scena sta davanti. Gli splat fuori da essa vengono densificati di rado."),
    NL("Het interessegebied dat deze run traint, als getint oppervlak met een gestippelde omtrek; vager waar de scène ervoor ligt. Splats erbuiten worden zelden verdicht."),
    RU("Область интереса этого обучения: тонированная поверхность с пунктирным контуром, бледнее там, где сцена перед ней. Сплаты вне её почти не уплотняются."),
    TR("Bu eğitimin ilgi bölgesi; renkli bir yüzey ve kesikli bir çerçeveyle çizilir, sahnenin önünde kaldığı yerde daha soluktur. Dışındaki splat'ler nadiren yoğunlaştırılır."));

SS_MSG(viewport_level_cameras,
    EN("auto-level"),    JA("自動水平"),      ZH_HANS("自动摆正"),  ZH_HANT("自動擺正"),
    KO("자동 수평"),      DE("autom. Ausrichtung"), FR("mise à niveau"),
    ES("nivelado autom."),
    PT("nivelamento autom."), IT("livellamento autom."), NL("autom. waterpas"),
    RU("автовыравнивание"), TR("otom. tesviye"));

SS_MSG(viewport_level_cameras_help,
    EN("Turn the view so the cameras' average up axis points up.\n- That is a guess, and a poor one for a tilted or upside-down 360 capture;\n- off uses the model's own axes, which a measured orientation has levelled."),
    JA("カメラの平均的な上方向が上を向くように視点を回転します。\n- これは推定であり、傾いた、または上下が逆の360度撮影では外れます。 オフにするとモデル自身の座標軸を使います。\n- 向きが実測されていれば、それはすでに水平です。"),
    ZH_HANS("旋转视图，使各相机的平均朝上方向指向上方。这只是推测，对倾斜或倒置的360 度拍摄往往不准；关闭后使用模型自身的坐标轴，若方向已实测则本就是水平的。"),
    ZH_HANT("旋轉檢視，使各相機的平均朝上方向指向上方。這只是推測，對傾斜或倒置的360 度拍攝往往不準；關閉後使用模型自身的座標軸，若方向已實測則本就是水平的。"),
    KO("카메라들의 평균 위쪽 축이 위를 향하도록 시점을 돌립니다.\n- 이는 추정이며 기울거나 뒤집힌 360도 촬영에서는 잘 맞지 않습니다.\n- 끄면 모델 자체의 축을 쓰며, 방향이 측정된 모델은 이미 수평입니다."),
    DE("Dreht die Ansicht so, dass die mittlere Oben-Achse der Kameras nach oben zeigt.\nDas ist geraten und bei einer geneigten oder kopfstehenden 360-Aufnahme schlecht; aus nutzt die Achsen des Modells."),
    FR("Oriente la vue pour que l'axe haut moyen des caméras pointe vers le haut.\n- C'est une supposition, mauvaise pour une capture 360 inclinée ou retournée ;\n- désactivé, la vue suit les axes propres du modèle."),
    ES("Gira la vista para que el eje superior medio de las cámaras apunte hacia arriba.\n- Es una suposición, mala en una captura 360 inclinada o invertida;\n- al desactivarla se usan los ejes propios del modelo."),
    PT("Roda a vista para que o eixo superior médio das câmaras aponte para cima.\nÉ uma suposição, má numa captura 360 inclinada ou invertida; desligada, usa os eixos do próprio modelo."),
    IT("Ruota la vista perché l'asse alto medio delle fotocamere punti verso l'alto.\nÈ una supposizione, sbagliata in una ripresa 360 inclinata o capovolta; disattivata, usa gli assi propri del modello."),
    NL("Draait het beeld zodat de gemiddelde omhoog-as van de camera's omhoog wijst.\n- Dat is een gok, en een slechte bij een gekantelde of omgekeerde 360-opname;\n- uit gebruikt de assen van het model zelf."),
    RU("Поворачивает вид так, чтобы средняя ось «вверх» камер смотрела вверх.\n- Это догадка, плохая для наклонной или перевёрнутой 360-съёмки;\n- выключено — используются собственные оси модели."),
    TR("Görünümü, kameraların ortalama yukarı ekseni yukarı bakacak biçimde döndürür.\nBu bir tahmindir ve eğik ya da ters 360 çekimlerde kötüdür; kapalıyken modelin kendi eksenleri kullanılır."));

SS_MSG(viewport_center,
    EN("center"),        JA("中心"),          ZH_HANS("中心"),     ZH_HANT("中心"),
    KO("중심"),           DE("Zentrum"),      FR("centre"),       ES("centro"),
    PT("centro"),        IT("centro"),       NL("centrum"),      RU("центр"),
    TR("merkez"));

SS_MSG(viewport_center_help,
    EN("The point the view orbits about and Reset view frames: a statistic of the cameras or of the points, or the model's own origin. This moves only the view, never the model."),
    JA("視点が回転する中心であり、ビューのリセットで画面に収める点です。カメラまたは点の統計値か、モデル自身の原点を選びます。動くのは視点だけで、モデルは動きません。"),
    ZH_HANS("视图绕其旋转、重置视图时对准的点：相机或点的某种统计量，或模型自身的原点。只移动视图，不移动模型。"),
    ZH_HANT("檢視繞其旋轉、重設檢視時對準的點：相機或點的某種統計量，或模型自身的原點。只移動檢視，不移動模型。"),
    KO("시점이 회전하는 중심이자 뷰 재설정이 맞추는 점입니다. 카메라나 점의 통계값, 또는 모델 자체의 원점 중에서 고릅니다. 시점만 움직이며 모델은 움직이지 않습니다."),
    DE("Orbit- und Rücksetzpunkt: Kamera- oder Punktstatistik oder Modellursprung. Ändert nur die Ansicht, nicht das Modell."),
    FR("Centre d’orbite et de réinitialisation : statistique des caméras ou points, ou origine du modèle. Déplace seulement la vue."),
    ES("El punto alrededor del cual gira la vista y que el reinicio encuadra: una estadística de las cámaras o de los puntos, o el origen del modelo. Solo mueve la vista, nunca el modelo."),
    PT("O ponto em torno do qual a vista gira e que o reinício enquadra: uma estatística das câmeras ou dos pontos, ou a origem do modelo. Só move a vista, nunca o modelo."),
    IT("Il punto attorno a cui ruota la vista e che il ripristino inquadra: una statistica delle camere o dei punti, oppure l'origine del modello. Sposta solo la vista, mai il modello."),
    NL("Middelpunt voor draaien en herstellen: camera- of puntstatistiek of modeloorsprong. Verplaatst alleen het beeld."),
    RU("Точка, вокруг которой вращается вид и на которую его наводит сброс: статистика камер или точек либо начало координат модели. Двигает только вид, но не модель."),
    TR("Dönüş ve sıfırlama merkezi: kamera veya nokta istatistiği ya da model başlangıcı. Yalnız görünümü değiştirir."));

SS_MSG(viewport_scale,
    EN("resolution"),    JA("解像度"),       ZH_HANS("分辨率"),   ZH_HANT("解析度"),
    KO("해상도"),        DE("Auflösung"),   FR("résolution"),  ES("resolución"),
    PT("resolução"),     IT("risoluzione"),  NL("resolutie"),    RU("разрешение"),
    TR("çözünürlük"));

SS_MSG(viewport_scale_auto,
    EN("Auto"),         JA("自動"),          ZH_HANS("自动"),    ZH_HANT("自動"),
    KO("자동"),          DE("Auto"),          FR("Auto"),        ES("Auto"),
    PT("Auto"),         IT("Auto"),          NL("Auto"),        RU("Авто"),
    TR("Oto"));

SS_MSG(viewport_fov_help,
    EN("Field of view, in degrees: how much of the scene the viewport takes "
       "in. This is the preview camera only -- it changes nothing about the "
       "dataset or the training."),
    JA("視野角（度）です。ビューポートにどれだけ広く写すかを決めます。"
       "プレビュー用のカメラだけの設定で、データセットや学習には影響しません。"),
    ZH_HANS("视场角（度）：视口能看进多大范围。这只是预览相机的设置，"
            "不会影响数据集或训练。"),
    ZH_HANT("視角（度）：檢視區能看進多大範圍。這只是預覽相機的設定，"
            "不會影響資料集或訓練。"),
    KO("시야각(도): 뷰포트가 장면을 얼마나 넓게 담을지 정합니다. 미리보기 "
       "카메라에만 적용되며 데이터셋이나 학습에는 영향이 없습니다."),
    DE("Blickfeld in Grad: wie viel von der Szene das Fenster erfasst. Nur die "
       "Vorschaukamera -- am Datensatz und am Training ändert das nichts."),
    FR("Champ de vision, en degrés : quelle part de la scène la vue embrasse. "
       "Caméra d'aperçu seulement -- cela ne change rien au jeu de données ni "
       "à l'entraînement."),
    ES("Campo de visión, en grados: cuánto de la escena abarca la vista. Solo "
       "afecta a la cámara de vista previa; no cambia nada del conjunto de "
       "datos ni del entrenamiento."),
    PT("Campo de visão, em graus: quanto da cena a vista abrange. Apenas a "
       "câmera de prévia -- não muda nada no conjunto de dados nem no "
       "treinamento."),
    IT("Campo visivo, in gradi: quanta scena entra nella vista. Riguarda solo "
       "la fotocamera di anteprima -- non cambia nulla del set di dati né "
       "dell'addestramento."),
    NL("Beeldhoek in graden: hoeveel van de scène het venster vangt. Alleen de "
       "voorbeeldcamera -- aan de dataset en de training verandert dit niets."),
    RU("Поле зрения в градусах: сколько сцены попадает в окно. Только камера "
       "предпросмотра — на набор данных и обучение это не влияет."),
    TR("Görüş alanı, derece: görünümün sahneden ne kadarını aldığı. Yalnızca "
       "önizleme kamerası -- veri kümesini de eğitimi de değiştirmez."));

SS_MSG(viewport_scale_help,
    EN("Render resolution relative to the viewport size. Lower is faster and steals less time from training.\nAuto drops to half while you move the camera and goes back to full once it settles."),
    JA("ビューポートの大きさに対する描画解像度です。下げるほど速くなり、学習から奪う時間も減ります。「自動」はカメラを動かしている間は半分に落とし、止まると元に戻します。"),
    ZH_HANS("相对于视口尺寸的渲染分辨率。调低更快，也少占用训练时间。“自动”会在你移动相机时降到一半，停下后恢复。"),
    ZH_HANT("相對於檢視區尺寸的算繪解析度。調低更快，也少佔用訓練時間。「自動」會在你移動相機時降到一半，停下後恢復。"),
    KO("뷰포트 크기에 대한 렌더 해상도입니다. 낮출수록 빠르고 학습 시간을 덜 가져갑니다. \"자동\"은 카메라를 움직이는 동안 절반으로 낮췄다가 멈추면 원래대로 돌아갑니다."),
    DE("Renderauflösung relativ zur Fenstergröße. Niedriger ist schneller und nimmt dem Training weniger Zeit weg.\nAuto halbiert sie, solange Sie die Kamera bewegen, und geht zurück auf voll, sobald sie steht."),
    FR("Résolution de rendu par rapport à la taille de la vue.\n- Plus bas est plus rapide et vole moins de temps à l'entraînement.\n- Auto descend à la moitié pendant que vous déplacez la caméra et remonte quand elle s'arrête."),
    ES("Resolución de render respecto al tamaño de la vista. Más baja es más rápida y quita menos tiempo al entrenamiento.\nAuto baja a la mitad mientras mueve la cámara y vuelve a plena cuando se detiene."),
    PT("Resolução de renderização em relação ao tamanho da vista.\n- Mais baixa é mais rápida e rouba menos tempo do treinamento.\n- Auto cai para metade enquanto você move a câmera e volta ao cheio quando ela para."),
    IT("Risoluzione di rendering rispetto alla dimensione della vista.\n- Più bassa è più rapida e ruba meno tempo all'addestramento.\n- Auto scende a metà mentre muove la fotocamera e torna piena quando si ferma."),
    NL("Renderresolutie ten opzichte van de venstergrootte. Lager is sneller en kost de training minder tijd.\nAuto halveert zolang u de camera beweegt en gaat terug naar vol zodra die stilstaat."),
    RU("Разрешение отрисовки относительно размера окна. Ниже — быстрее и меньше отнимает времени у обучения.\n«Авто» снижает его вдвое, пока вы двигаете камеру, и возвращает полное, когда она замирает."),
    TR("Görünüm boyutuna göre işleme çözünürlüğü. Düşük olan daha hızlıdır ve eğitimden daha az zaman çalar.\n\"Oto\", kamerayı hareket ettirdiğiniz sürece yarıya iner ve durunca tama döner."));

SS_MSG(viewport_live,
    EN("live"),          JA("ライブ"),        ZH_HANS("实时"),     ZH_HANT("即時"),
    KO("실시간"),         DE("live"),         FR("direct"),       ES("en vivo"),
    PT("ao vivo"),       IT("dal vivo"),     NL("live"),         RU("вживую"),
    TR("canlı"));

SS_MSG(viewport_live_help,
    EN("Continuously re-render while training so the viewport follows the "
       "optimization."),
    JA("学習中も描画を更新し続け、最適化の様子をビューポートで追えるようにします。"),
    ZH_HANS("训练时持续重新渲染，让视口跟随优化过程。"),
    ZH_HANT("訓練時持續重新算繪，讓檢視區跟隨最佳化過程。"),
    KO("학습 중에도 계속 다시 렌더링해서 뷰포트가 최적화를 따라가게 합니다."),
    DE("Während des Trainings laufend neu rendern, damit das Fenster der "
       "Optimierung folgt."),
    FR("Rendre en continu pendant l'entraînement pour que la vue suive "
       "l'optimisation."),
    ES("Volver a renderizar continuamente durante el entrenamiento para que "
       "la vista siga la optimización."),
    PT("Renderizar continuamente durante o treinamento para que a vista "
       "acompanhe a otimização."),
    IT("Rigenerare l'immagine di continuo durante l'addestramento, così la "
       "vista segue l'ottimizzazione."),
    NL("Tijdens de training doorlopend opnieuw renderen zodat het beeld de "
       "optimalisatie volgt."),
    RU("Постоянно перерисовывать во время обучения, чтобы окно следовало за "
       "оптимизацией."),
    TR("Eğitim sırasında sürekli yeniden işleyerek görünümün iyileştirmeyi "
       "izlemesini sağlar."));

SS_MSG(viewport_reset_view,
    EN("Reset view"),    JA("視点をリセット"), ZH_HANS("重置视角"),  ZH_HANT("重設視角"),
    KO("시점 초기화"),    DE("Ansicht zurücksetzen"), FR("Réinitialiser la vue"),
    ES("Restablecer la vista"), PT("Redefinir a vista"),
    IT("Reimposta la vista"), NL("Weergave herstellen"), RU("Сбросить вид"),
    TR("Görünümü sıfırla"));

// The four navigation modes and the four projections, as the viewport's two
// combo boxes name them. Translated, even though the web viewer's own UI is
// English: a reader of the interface should not have to know English to tell
// an orbit from a flythrough. viewport_nav_help names them by substitution
// ({0}..{3}) so the tooltip and the combo cannot drift apart.

SS_MSG(nav_turntable,
    EN("Turntable"),    JA("ターンテーブル"),  ZH_HANS("转台"),    ZH_HANT("轉台"),
    KO("턴테이블"),      DE("Drehteller"),    FR("Table tournante"),
    ES("Plato giratorio"), PT("Mesa giratória"), IT("Piatto rotante"),
    NL("Draaitafel"),   RU("Поворотный стол"), TR("Döner tabla"));

SS_MSG(nav_trackball,
    EN("Trackball"),    JA("トラックボール"),  ZH_HANS("轨迹球"),  ZH_HANT("軌跡球"),
    KO("트랙볼"),        DE("Trackball"),     FR("Trackball"),   ES("Trackball"),
    PT("Trackball"),    IT("Trackball"),     NL("Trackball"),   RU("Трекбол"),
    TR("Trackball"));

SS_MSG(nav_first_person,
    EN("First person"), JA("一人称視点"),     ZH_HANS("第一人称"), ZH_HANT("第一人稱"),
    KO("1인칭"),         DE("Ego-Perspektive"), FR("Première personne"),
    ES("Primera persona"), PT("Primeira pessoa"), IT("Prima persona"),
    NL("Eerste persoon"), RU("От первого лица"), TR("Birinci şahıs"));

SS_MSG(nav_free_fly,
    EN("Free fly"),     JA("フリーフライト"),  ZH_HANS("自由飞行"), ZH_HANT("自由飛行"),
    KO("자유 비행"),     DE("Freiflug"),      FR("Vol libre"),   ES("Vuelo libre"),
    PT("Voo livre"),    IT("Volo libero"),   NL("Vrij vliegen"), RU("Свободный полёт"),
    TR("Serbest uçuş"));

SS_MSG(cam_perspective,
    EN("Perspective"),  JA("透視投影"),       ZH_HANS("透视"),    ZH_HANT("透視"),
    KO("원근"),          DE("Perspektivisch"), FR("Perspective"), ES("Perspectiva"),
    PT("Perspectiva"),  IT("Prospettica"),   NL("Perspectief"), RU("Перспектива"),
    TR("Perspektif"));

SS_MSG(cam_fisheye_equidistant,
    EN("Fisheye (equidistant)"),
    JA("魚眼（等距離射影）"),
    ZH_HANS("鱼眼（等距）"),
    ZH_HANT("魚眼（等距）"),
    KO("어안(등거리)"),
    DE("Fisheye (äquidistant)"),
    FR("Fisheye (équidistant)"),
    ES("Ojo de pez (equidistante)"),
    PT("Olho de peixe (equidistante)"),
    IT("Fisheye (equidistante)"),
    NL("Fisheye (equidistant)"),
    RU("Фишай (эквидистантный)"),
    TR("Balıkgözü (eşit uzaklık)"));

SS_MSG(cam_fisheye_equisolid,
    EN("Fisheye (equisolid)"),
    JA("魚眼（等立体角射影）"),
    ZH_HANS("鱼眼（等立体角）"),
    ZH_HANT("魚眼（等立體角）"),
    KO("어안(등입체각)"),
    DE("Fisheye (flächentreu)"),
    FR("Fisheye (équisolide)"),
    ES("Ojo de pez (equisólido)"),
    PT("Olho de peixe (equissólido)"),
    IT("Fisheye (equisolido)"),
    NL("Fisheye (equisolide)"),
    RU("Фишай (равновеликий)"),
    TR("Balıkgözü (eşit alan)"));

SS_MSG(cam_equirectangular,
    EN("Equirectangular (360°)"),
    JA("正距円筒（360度）"),
    ZH_HANS("等距柱状（360 度）"),
    ZH_HANT("等距柱狀（360 度）"),
    KO("정거원통(360도)"),
    DE("Äquirektangulär (360°)"),
    FR("Équirectangulaire (360°)"),
    ES("Equirrectangular (360°)"),
    PT("Equirretangular (360°)"),
    IT("Equirettangolare (360°)"),
    NL("Equirectangulair (360°)"),
    RU("Эквиректангулярная (360°)"),
    TR("Eş dikdörtgen (360°)"));

SS_MSG(viewport_nav_help,
    EN("Navigation mode (identical to the web viewer):\n- {0} / {1} -- LMB orbit, RMB/MMB/Shift pan, wheel zoom ({1}: E/Q roll)\n- {2} / {3} -- LMB look, WASD/arrows move, E/Q up-down ({3}: E/Q roll)\n- Gamepad: left stick move, right stick look, triggers up-down/roll."),
    JA("操作モードです（ウェブビューアと同じ）:\n- {0} / {1} -- 左ドラッグで回転、右・中ドラッグや Shift で平行移動、ホイールでズーム（{1}では E/Q はロール）\n- {2} / {3} -- 左ドラッグで視線、WASD／矢印で移動、E/Q で上下（{3}では E/Q はロール）\n- ゲームパッド: 左スティックで移動、右スティックで視線、トリガーで上下・ロール。"),
    ZH_HANS("导航模式（与网页查看器一致）：\n- {0} / {1} —— 左键环绕，右键／中键／Shift 平移，滚轮缩放（{1}下 E/Q 为滚转）\n- {2} / {3} —— 左键转视角，WASD／方向键移动，E/Q 升降（{3}下 E/Q 为滚转）\n- 手柄：左摇杆移动，右摇杆转视角，扳机升降／滚转。"),
    ZH_HANT("導覽模式（與網頁檢視器一致）：\n- {0} / {1} —— 左鍵環繞，右鍵／中鍵／Shift 平移，滾輪縮放（{1}下 E/Q 為滾轉）\n- {2} / {3} —— 左鍵轉視角，WASD／方向鍵移動，E/Q 升降（{3}下 E/Q 為滾轉）\n- 手把：左搖桿移動，右搖桿轉視角，扳機升降／滾轉。"),
    KO("이동 방식입니다(웹 뷰어와 동일):\n- {0} / {1} -- 왼쪽 드래그로 궤도 회전, 오른쪽·가운데·Shift로 이동, 휠로 확대·축소({1}에서는 E/Q가 롤)\n- {2} / {3} -- 왼쪽 드래그로 시선, WASD·화살표로 이동, E/Q로 상하({3}에서는 E/Q가 롤)\n- 게임패드: 왼쪽 스틱 이동, 오른쪽 스틱 시선, 트리거 상하·롤."),
    DE("Navigation wie im Web-Betrachter.\n- {0} / {1}: links umkreisen, rechts/mittig/Umschalt schwenken, Rad zoomen ({1}: E/Q rollen).\n- {2} / {3}: links umsehen, WASD/Pfeile bewegen, E/Q hoch/runter ({3}: rollen).\n- Gamepad: linker Stick bewegt, rechter schaut, Trigger hoch/runter oder rollen."),
    FR("Navigation identique à la visionneuse web.\n- {0} / {1} : clic gauche orbite, droit/milieu/Maj translate, molette zoome ({1} : roulis E/Q).\n- {2} / {3} : clic gauche regarde, WASD/flèches déplacent, E/Q monte/descend ({3} : roulis).\n- Manette : stick gauche déplace, droit regarde, gâchettes hauteur/roulis."),
    ES("Navegación igual que en el visor web.\n- {0} / {1}: izq. orbita, der./centro/Mayús desplaza, rueda acerca ({1}: alabeo E/Q).\n- {2} / {3}: izq. mira, WASD/flechas mueven, E/Q sube/baja ({3}: alabeo).\n- Mando: stick izq. mueve, der. mira, gatillos ajustan altura o alabeo."),
    PT("Modo de navegação (igual ao visualizador web):\n- {0} / {1} -- botão esquerdo orbita, direito/meio ou Shift desloca, roda aproxima (em {1}, E/Q rolam)\n- {2} / {3} -- botão esquerdo olha, WASD/setas movem, E/Q sobem e descem (em {3}, E/Q rolam)\n- Controle: analógico esquerdo move, direito olha, gatilhos sobem-descem/rolam."),
    IT("Navigazione come nel visualizzatore web.\n- {0} / {1}: sinistro orbita, destro/centro/Maiusc trasla, rotella zoom ({1}: rollio E/Q).\n- {2} / {3}: sinistro guarda, WASD/frecce muovono, E/Q su/giù ({3}: rollio).\n- Gamepad: levetta sinistra muove, destra guarda, grilletti su/giù o rollio."),
    NL("Navigatiemodus (gelijk aan de webviewer):\n- {0} / {1} -- linkermuisknop draaien, rechter/midden of Shift verschuiven, wiel zoomen ({1}: E/Q rollen)\n- {2} / {3} -- linkermuisknop kijken, WASD/pijlen bewegen, E/Q omhoog-omlaag ({3}: E/Q rollen)\n- Gamepad: linkerstick bewegen, rechterstick kijken, triggers omhoog-omlaag/rollen."),
    RU("Навигация как в веб-просмотрщике.\n- {0} / {1}: левая кнопка вращает, правая/средняя/Shift сдвигает, колесо приближает ({1}: крен E/Q).\n- {2} / {3}: левая кнопка смотрит, WASD/стрелки перемещают, E/Q вверх/вниз ({3}: крен).\n- Геймпад: левый стик движет, правый поворачивает, триггеры меняют высоту или крен."),
    TR("Gezinme web görüntüleyicisindeki gibidir.\n- {0} / {1}: sol tuş döndürür, sağ/orta/Shift kaydırır, tekerlek yakınlaştırır ({1}: E/Q yalpa).\n- {2} / {3}: sol tuş bakar, WASD/oklar hareket eder, E/Q yukarı/aşağı ({3}: yalpa).\n- Oyun kolu: sol çubuk hareket, sağ çubuk bakış, tetikler yükseklik/yalpa."));

SS_MSG(viewport_speed_help,
    EN("Move speed for pan / keyboard / gamepad (log scale; the web viewer's "
       "Move Speed slider)."),
    JA("平行移動・キーボード・ゲームパッドの移動速度です（対数スケール。"
       "ウェブビューアの Move Speed スライダーと同じ）。"),
    ZH_HANS("平移／键盘／手柄的移动速度（对数刻度；即网页查看器的 Move Speed 滑块）。"),
    ZH_HANT("平移／鍵盤／手把的移動速度（對數刻度；即網頁檢視器的 Move Speed 滑桿）。"),
    KO("이동·키보드·게임패드의 이동 속도입니다(로그 눈금, 웹 뷰어의 Move Speed "
       "슬라이더와 같습니다)."),
    DE("Bewegungsgeschwindigkeit für Schwenken, Tastatur und Gamepad "
       "(logarithmisch; der Move-Speed-Regler des Web-Betrachters)."),
    FR("Vitesse de déplacement pour la translation, le clavier et la manette "
       "(échelle logarithmique ; le curseur Move Speed de la visionneuse web)."),
    ES("Velocidad de desplazamiento para el paneo, el teclado y el mando "
       "(escala logarítmica; el deslizador Move Speed del visor web)."),
    PT("Velocidade de deslocamento para o pan, o teclado e o controle (escala "
       "logarítmica; o controle deslizante Move Speed do visualizador web)."),
    IT("Velocità di spostamento per traslazione, tastiera e gamepad (scala "
       "logaritmica; il cursore Move Speed del visualizzatore web)."),
    NL("Bewegingssnelheid voor verschuiven, toetsenbord en gamepad "
       "(logaritmisch; de Move Speed-schuif van de webviewer)."),
    RU("Скорость перемещения для сдвига, клавиатуры и геймпада "
       "(логарифмическая шкала; ползунок Move Speed в веб-просмотрщике)."),
    TR("Kaydırma, klavye ve oyun kolu için hareket hızı (logaritmik ölçek; web "
       "görüntüleyicisindeki Move Speed sürgüsü)."));

SS_MSG(viewport_projection_help,
    EN("Projection used for the viewport (preview and training render)."),
    JA("ビューポート（プレビューと学習中の描画）で使う投影方式です。"),
    ZH_HANS("视口（预览与训练渲染）使用的投影方式。"),
    ZH_HANT("檢視區（預覽與訓練算繪）使用的投影方式。"),
    KO("뷰포트(미리보기와 학습 렌더링)에 쓰는 투영 방식입니다."),
    DE("Projektion für das Fenster (Vorschau und Trainingsdarstellung)."),
    FR("Projection utilisée pour la vue (aperçu et rendu d'entraînement)."),
    ES("Proyección usada en la vista (vista previa y render de entrenamiento)."),
    PT("Projeção usada na vista (prévia e renderização de treinamento)."),
    IT("Proiezione usata per la vista (anteprima e rendering di "
       "addestramento)."),
    NL("Projectie voor het beeld (voorbeeld en trainingsweergave)."),
    RU("Проекция для окна (предпросмотр и отрисовка при обучении)."),
    TR("Görünüm için kullanılan izdüşüm (önizleme ve eğitim işlemesi)."));

SS_MSG(viewport_open_a_dataset,
    EN("Open a dataset to see it here"),
    JA("データセットを開くとここに表示されます"),
    ZH_HANS("打开一个数据集就会显示在这里"),
    ZH_HANT("開啟一個資料集就會顯示在這裡"),
    KO("데이터셋을 열면 여기에 표시됩니다"),
    DE("Einen Datensatz öffnen, um ihn hier zu sehen"),
    FR("Ouvrez un jeu de données pour le voir ici"),
    ES("Abra un conjunto de datos para verlo aquí"),
    PT("Abra um conjunto de dados para vê-lo aqui"),
    IT("Apra un set di dati per vederlo qui"),
    NL("Open een dataset om die hier te zien"),
    RU("Откройте набор данных, чтобы увидеть его здесь"),
    TR("Burada görmek için bir veri kümesi açın"));

SS_MSG(viewport_rendering,
    EN("Rendering..."),  JA("描画しています…"), ZH_HANS("正在渲染…"), ZH_HANT("正在算繪…"),
    KO("렌더링 중…"),     DE("Wird gerendert …"), FR("Rendu en cours…"),
    ES("Renderizando…"), PT("Renderizando…"), IT("Rendering in corso…"),
    NL("Bezig met renderen…"), RU("Отрисовка…"), TR("İşleniyor…"));

SS_MSG(viewport_render_failed,
    EN("preview render failed"),
    JA("プレビューの描画に失敗しました"),
    ZH_HANS("预览渲染失败"),
    ZH_HANT("預覽算繪失敗"),
    KO("미리보기 렌더링에 실패했습니다"),
    DE("Vorschau konnte nicht gerendert werden"),
    FR("échec du rendu de l'aperçu"),
    ES("falló el render de la vista previa"),
    PT("falha ao renderizar a prévia"),
    IT("rendering dell'anteprima non riuscito"),
    NL("voorbeeldweergave mislukt"),
    RU("не удалось отрисовать предпросмотр"),
    TR("önizleme işlenemedi"));

SS_MSG(viewport_render_error,
    EN("render error: {0}"),
    JA("描画エラー: {0}"),
    ZH_HANS("渲染错误：{0}"),
    ZH_HANT("算繪錯誤：{0}"),
    KO("렌더링 오류: {0}"),
    DE("Renderfehler: {0}"),
    FR("erreur de rendu : {0}"),
    ES("error de render: {0}"),
    PT("erro de renderização: {0}"),
    IT("errore di rendering: {0}"),
    NL("renderfout: {0}"),
    RU("ошибка отрисовки: {0}"),
    TR("işleme hatası: {0}"));

// ===========================================================================
// Config editor (the "All Options" table)
// ===========================================================================

SS_MSG(cfg_tier_basic,
    EN("Basic"),         JA("基本"),          ZH_HANS("基本"),     ZH_HANT("基本"),
    KO("기본"),           DE("Basis"),        FR("Essentiel"),    ES("Básico"),
    PT("Básico"),        IT("Base"),         NL("Basis"),        RU("Основные"),
    TR("Temel"));

SS_MSG(cfg_tier_advanced,
    EN("Advanced"),      JA("詳細"),          ZH_HANS("进阶"),     ZH_HANT("進階"),
    KO("고급"),           DE("Erweitert"),    FR("Avancé"),       ES("Avanzado"),
    PT("Avançado"),      IT("Avanzate"),     NL("Geavanceerd"),  RU("Дополнительные"),
    TR("Gelişmiş"));

SS_MSG(cfg_tier_all,
    EN("Everything"),    JA("すべて"),        ZH_HANS("全部"),     ZH_HANT("全部"),
    KO("전체"),           DE("Alles"),        FR("Tout"),         ES("Todo"),
    PT("Tudo"),          IT("Tutto"),        NL("Alles"),        RU("Все"),
    TR("Tümü"));

SS_MSG(cfg_tier_help,
    EN("How specialist an option may be and still be listed. Searching looks "
       "through all of them either way."),
    JA("一覧に出す設定の細かさです。検索はどの設定でも対象になります。"),
    ZH_HANS("列出多少高级选项。搜索始终覆盖全部选项。"),
    ZH_HANT("列出多少進階選項。搜尋始終涵蓋全部選項。"),
    KO("목록에 나오는 옵션의 전문성 수준입니다. 검색은 언제나 전체를 대상으로 합니다."),
    DE("Wie speziell eine Einstellung sein darf, um noch gelistet zu werden. "
       "Die Suche geht ohnehin durch alle."),
    FR("Jusqu'à quel point une option peut être spécialisée et rester listée. "
       "La recherche parcourt toutes les options."),
    ES("Hasta qué punto una opción puede ser especializada y seguir listada. "
       "La búsqueda las recorre todas."),
    PT("Até que ponto uma opção pode ser especializada e continuar listada. "
       "A busca percorre todas."),
    IT("Quanto può essere specialistica un'opzione e restare in elenco. "
       "La ricerca le attraversa tutte."),
    NL("Hoe specialistisch een optie mag zijn om nog te worden getoond. "
       "Zoeken doorloopt ze hoe dan ook allemaal."),
    RU("Насколько узкоспециальным может быть параметр, чтобы попасть в список. "
       "Поиск всё равно идёт по всем."),
    TR("Bir seçenek listede kalabilmek için ne kadar uzmanlaşmış olabilir. "
       "Arama yine de hepsini tarar."));

SS_MSG(cfg_search_hint,
    EN("search options (name or description)"),
    JA("設定を検索（名前または説明）"),
    ZH_HANS("搜索选项（名称或说明）"),
    ZH_HANT("搜尋選項（名稱或說明）"),
    KO("옵션 검색(이름 또는 설명)"),
    DE("Einstellungen durchsuchen (Name oder Beschreibung)"),
    FR("rechercher une option (nom ou description)"),
    ES("buscar opciones (nombre o descripción)"),
    PT("pesquisar opções (nome ou descrição)"),
    IT("cerca opzioni (nome o descrizione)"),
    NL("opties zoeken (naam of beschrijving)"),
    RU("поиск параметров (имя или описание)"),
    TR("seçenek ara (ad veya açıklama)"));

SS_MSG(cfg_edited_only,
    EN("edited"),        JA("変更済み"),      ZH_HANS("已修改"),   ZH_HANT("已修改"),
    KO("변경됨"),         DE("geändert"),     FR("modifiées"),    ES("editadas"),
    PT("editadas"),      IT("modificate"),   NL("gewijzigd"),    RU("изменённые"),
    TR("değiştirilen"));

SS_MSG(cfg_edited_only_help,
    EN("Show only options changed from the preset default."),
    JA("プリセットの既定値から変更した設定だけを表示します。"),
    ZH_HANS("只显示与预设默认值不同的选项。"),
    ZH_HANT("只顯示與預設值不同的選項。"),
    KO("프리셋 기본값에서 바뀐 옵션만 보여줍니다."),
    DE("Nur Einstellungen zeigen, die von der Voreinstellung abweichen."),
    FR("N'afficher que les options différentes de la valeur du préréglage."),
    ES("Mostrar solo las opciones que difieren del valor del ajuste."),
    PT("Mostrar apenas as opções diferentes do valor da predefinição."),
    IT("Mostrare solo le opzioni diverse dal valore della preimpostazione."),
    NL("Alleen opties tonen die afwijken van de voorinstelling."),
    RU("Показывать только параметры, отличающиеся от значения пресета."),
    TR("Yalnızca hazır ayarın varsayılanından farklı seçenekleri göster."));

SS_MSG(cfg_no_description,
    EN("(no description)"),
    JA("（説明なし）"),   ZH_HANS("（无说明）"), ZH_HANT("（無說明）"),
    KO("(설명 없음)"),   DE("(keine Beschreibung)"), FR("(pas de description)"),
    ES("(sin descripción)"), PT("(sem descrição)"), IT("(nessuna descrizione)"),
    NL("(geen beschrijving)"), RU("(без описания)"), TR("(açıklama yok)"));

SS_MSG(cfg_preset_default,
    EN("preset default: {0}"),
    JA("プリセットの既定値: {0}"),
    ZH_HANS("预设默认值：{0}"),
    ZH_HANT("預設值：{0}"),
    KO("프리셋 기본값: {0}"),
    DE("Voreinstellung: {0}"),
    FR("valeur du préréglage : {0}"),
    ES("valor del ajuste: {0}"),
    PT("valor da predefinição: {0}"),
    IT("valore della preimpostazione: {0}"),
    NL("standaard van voorinstelling: {0}"),
    RU("значение пресета: {0}"),
    TR("hazır ayar varsayılanı: {0}"));

SS_MSG(cfg_reset_to,
    EN("Reset to {0}"),  JA("{0} に戻す"),    ZH_HANS("重置为 {0}"), ZH_HANT("重設為 {0}"),
    KO("{0}(으)로 되돌리기"), DE("Auf {0} zurücksetzen"),
    FR("Réinitialiser à {0}"), ES("Restablecer a {0}"),
    PT("Redefinir para {0}"), IT("Reimposta a {0}"), NL("Terugzetten op {0}"),
    RU("Вернуть к {0}"), TR("{0} değerine sıfırla"));

SS_MSG(cfg_auto,
    EN("(auto)"),        JA("（自動）"),      ZH_HANS("（自动）"),  ZH_HANT("（自動）"),
    KO("(자동)"),         DE("(automatisch)"), FR("(auto)"),      ES("(automático)"),
    PT("(automático)"),  IT("(automatico)"), NL("(automatisch)"), RU("(авто)"),
    TR("(otomatik)"));

SS_MSG(cfg_unchecked_is_auto,
    EN("unchecked = auto"),
    JA("チェックを外すと自動"),
    ZH_HANS("未勾选 = 自动"),
    ZH_HANT("未勾選 = 自動"),
    KO("체크 해제 = 자동"),
    DE("nicht angehakt = automatisch"),
    FR("décoché = automatique"),
    ES("sin marcar = automático"),
    PT("desmarcado = automático"),
    IT("deselezionato = automatico"),
    NL("niet aangevinkt = automatisch"),
    RU("без флажка — авто"),
    TR("işaretsiz = otomatik"));

// ===========================================================================
// File dialog
// ===========================================================================

SS_MSG(fd_up,
    EN("Up"),            JA("上へ"),          ZH_HANS("上一级"),   ZH_HANT("上一層"),
    KO("위로"),           DE("Aufwärts"),     FR("Dossier parent"), ES("Subir"),
    PT("Acima"),         IT("Su"),           NL("Omhoog"),       RU("Вверх"),
    TR("Yukarı"));

SS_MSG(fd_home,
    EN("Home"),          JA("ホーム"),        ZH_HANS("主目录"),   ZH_HANT("主目錄"),
    KO("홈"),             DE("Persönlicher Ordner"), FR("Dossier personnel"),
    ES("Carpeta personal"), PT("Pasta pessoal"), IT("Cartella personale"),
    NL("Persoonlijke map"), RU("Домашняя папка"), TR("Ana klasör"));

SS_MSG(fd_drive,
    EN("Drive"),         JA("ドライブ"),      ZH_HANS("驱动器"),   ZH_HANT("磁碟機"),
    KO("드라이브"),       DE("Laufwerk"),     FR("Lecteur"),      ES("Unidad"),
    PT("Unidade"),       IT("Unità"),        NL("Station"),      RU("Диск"),
    TR("Sürücü"));

SS_MSG(fd_select_highlighted,
    EN("Select Highlighted Folder"),
    JA("選択中のフォルダを使う"),
    ZH_HANS("选择高亮的文件夹"),
    ZH_HANT("選擇反白的資料夾"),
    KO("선택한 폴더 사용"),
    DE("Markierten Ordner wählen"),
    FR("Choisir le dossier sélectionné"),
    ES("Elegir la carpeta resaltada"),
    PT("Escolher a pasta destacada"),
    IT("Scegli la cartella evidenziata"),
    NL("Gemarkeerde map kiezen"),
    RU("Выбрать выделенную папку"),
    TR("Seçili klasörü kullan"));

SS_MSG(fd_use_this_folder,
    EN("Use This Folder"),
    JA("このフォルダを使う"),
    ZH_HANS("使用当前文件夹"),
    ZH_HANT("使用目前資料夾"),
    KO("이 폴더 사용"),
    DE("Diesen Ordner verwenden"),
    FR("Utiliser ce dossier"),
    ES("Usar esta carpeta"),
    PT("Usar esta pasta"),
    IT("Usa questa cartella"),
    NL("Deze map gebruiken"),
    RU("Использовать эту папку"),
    TR("Bu klasörü kullan"));

SS_MSG(fd_select_file,
    EN("Select File"),   JA("ファイルを選択"), ZH_HANS("选择文件"),  ZH_HANT("選擇檔案"),
    KO("파일 선택"),      DE("Datei wählen"), FR("Choisir le fichier"),
    ES("Elegir el archivo"), PT("Escolher o arquivo"), IT("Scegli il file"),
    NL("Bestand kiezen"), RU("Выбрать файл"), TR("Dosya seç"));

// {0} is a count. Labelled, not inflected -- see the plural rule above.
SS_MSG(fd_select_files,
    EN("Select Files ({0})"),
    JA("ファイルを選択（{0} 件）"),
    ZH_HANS("选择文件（{0} 个）"),
    ZH_HANT("選擇檔案（{0} 個）"),
    KO("파일 선택({0}개)"),
    DE("Dateien wählen ({0})"),
    FR("Choisir les fichiers ({0})"),
    ES("Elegir los archivos ({0})"),
    PT("Escolher os arquivos ({0})"),
    IT("Scegli i file ({0})"),
    NL("Bestanden kiezen ({0})"),
    RU("Выбрать файлы ({0})"),
    TR("Dosyaları seç ({0})"));

SS_MSG(fd_save_here,
    EN("Save"),          JA("保存"),          ZH_HANS("保存"),     ZH_HANT("儲存"),
    KO("저장"),           DE("Speichern"),    FR("Enregistrer"),
    ES("Guardar"),       PT("Salvar"),        IT("Salva"),
    NL("Opslaan"),       RU("Сохранить"),     TR("Kaydet"));

SS_MSG(fd_file_name,
    EN("File name"),     JA("ファイル名"),     ZH_HANS("文件名"),   ZH_HANT("檔案名稱"),
    KO("파일 이름"),      DE("Dateiname"),    FR("Nom du fichier"),
    ES("Nombre del archivo"), PT("Nome do arquivo"), IT("Nome del file"),
    NL("Bestandsnaam"),  RU("Имя файла"),     TR("Dosya adı"));

SS_MSG(fd_will_replace,
    EN("A file of that name is there and will be replaced."),
    JA("同じ名前のファイルがあり、置き換えられます。"),
    ZH_HANS("同名文件已存在，将被替换。"),
    ZH_HANT("同名檔案已存在，將被取代。"),
    KO("같은 이름의 파일이 있어 덮어씁니다."),
    DE("Eine Datei dieses Namens ist vorhanden und wird ersetzt."),
    FR("Un fichier de ce nom existe et sera remplacé."),
    ES("Ya hay un archivo con ese nombre y será reemplazado."),
    PT("Já existe um arquivo com esse nome e ele será substituído."),
    IT("Esiste già un file con quel nome e verrà sostituito."),
    NL("Er is al een bestand met die naam; het wordt vervangen."),
    RU("Файл с таким именем есть и будет заменён."),
    TR("Bu adda bir dosya var ve değiştirilecek."));

SS_MSG(fd_multi_hint,
    EN("(click several to add them all)"),
    JA("（複数クリックするとまとめて追加できます）"),
    ZH_HANS("（可以点选多个，一次全部加入）"),
    ZH_HANT("（可以點選多個，一次全部加入）"),
    KO("(여러 개를 클릭하면 모두 추가됩니다)"),
    DE("(mehrere anklicken, um alle zu übernehmen)"),
    FR("(cliquez-en plusieurs pour tous les ajouter)"),
    ES("(haga clic en varios para añadirlos todos)"),
    PT("(clique em vários para adicioná-los todos)"),
    IT("(ne clicchi più d'uno per aggiungerli tutti)"),
    NL("(klik er meerdere aan om ze allemaal toe te voegen)"),
    RU("(кликните несколько, чтобы добавить их все)"),
    TR("(hepsini eklemek için birkaçına tıklayın)"));

SS_MSG(cancel,
    EN("Cancel"),        JA("キャンセル"),    ZH_HANS("取消"),     ZH_HANT("取消"),
    KO("취소"),           DE("Abbrechen"),    FR("Annuler"),      ES("Cancelar"),
    PT("Cancelar"),      IT("Annulla"),      NL("Annuleren"),    RU("Отмена"),
    TR("İptal"));

// ===========================================================================
// Viewer screen -- a finished model, open for looking at
// ===========================================================================

SS_MSG(menu_open_splat,
    EN("Open a Splat File..."),
    JA("スプラットファイルを開く…"),
    ZH_HANS("打开泼溅文件…"),
    ZH_HANT("開啟潑濺檔案…"),
    KO("스플랫 파일 열기…"),
    DE("Splat-Datei öffnen …"),
    FR("Ouvrir un fichier de splats…"),
    ES("Abrir un archivo de splats…"),
    PT("Abrir um arquivo de splats…"),
    IT("Apri un file di splat…"),
    NL("Splatbestand openen…"),
    RU("Открыть файл сплатов…"),
    TR("Splat dosyası aç…"));

SS_MSG(home_open_splat,
    EN("Open a Model or Reconstruction"),
    JA("モデルまたは再構成を開く"),
    ZH_HANS("打开模型或重建"),
    ZH_HANT("開啟模型或重建"),
    KO("모델 또는 복원 열기"),
    DE("Modell oder Rekonstruktion öffnen"),
    FR("Ouvrir un modèle ou une reconstruction"),
    ES("Abrir un modelo o una reconstrucción"),
    PT("Abrir um modelo ou uma reconstrução"),
    IT("Apri un modello o una ricostruzione"),
    NL("Een model of reconstructie openen"),
    RU("Открыть модель или реконструкцию"),
    TR("Bir model veya yeniden yapım aç"));

SS_MSG(home_open_splat_help,
    EN("A .ply file, a checkpoint, a run folder, or a reconstruction -- its dataset folder, or any one of the files that define it. Look around it, and clean it up."),
    JA("PLY、チェックポイント、実行フォルダ、または再構成のデータセットフォルダや構成ファイルを開いて、表示・編集します。"),
    ZH_HANS("可以是 .ply 文件、检查点、运行文件夹，或一个重建——它的数据集文件夹，或定义它的任意一个文件。可以随意观察，也可以清理。"),
    ZH_HANT("可以是 .ply 檔案、檢查點、執行資料夾，或一個重建——它的資料集資料夾，或定義它的任意一個檔案。可以隨意觀察，也可以清理。"),
    KO(".ply 파일, 체크포인트, 실행 폴더, 또는 복원 -- 그 데이터셋 폴더나 그것을 이루는 파일 중 하나. 둘러보고 정리할 수 있습니다."),
    DE("Eine .ply-Datei, ein Prüfpunkt, ein Laufordner oder eine Rekonstruktion -- ihr Datensatzordner oder eine der Dateien, die sie ausmachen. Darin umsehen und aufräumen."),
    FR("Ouvrir et nettoyer un .ply, un point de contrôle, un dossier d’exécution ou une reconstruction (dossier ou fichier du modèle)."),
    ES("Abre y limpia un .ply, un punto de control, una carpeta de ejecución o una reconstrucción (carpeta o archivo del modelo)."),
    PT("Abra e limpe um .ply, checkpoint, pasta de execução ou reconstrução (pasta ou ficheiro do modelo)."),
    IT("Apri e ripulisci un .ply, checkpoint, cartella di esecuzione o ricostruzione (cartella o file del modello)."),
    NL("Open en bewerk een .ply, checkpoint, run-map of reconstructie (datasetmap of een modelbestand)."),
    RU("Файл .ply, контрольная точка, папка запуска или реконструкция -- её папка набора данных или любой из файлов, которые её задают. Чтобы осмотреться и чтобы почистить."),
    TR("Bir .ply, kontrol noktası, çalışma klasörü veya yeniden yapımı (veri klasörü ya da model dosyası) açıp temizleyin."));

SS_MSG(viewer_pick_file,
    EN("Choose a splat file, checkpoint or run folder"),
    JA("スプラットファイル・チェックポイント・実行フォルダを選択"),
    ZH_HANS("选择泼溅文件、检查点或运行文件夹"),
    ZH_HANT("選擇潑濺檔案、檢查點或執行資料夾"),
    KO("스플랫 파일, 체크포인트 또는 실행 폴더 선택"),
    DE("Splat-Datei, Prüfpunkt oder Laufordner wählen"),
    FR("Choisir un fichier de splats, un point de sauvegarde ou un dossier d'exécution"),
    ES("Elija un archivo de splats, un punto de control o una carpeta de ejecución"),
    PT("Escolha um arquivo de splats, um ponto de verificação ou uma pasta de execução"),
    IT("Scelga un file di splat, un punto di controllo o una cartella di esecuzione"),
    NL("Kies een splatbestand, checkpoint of uitvoermap"),
    RU("Выберите файл сплатов, контрольную точку или папку запуска"),
    TR("Bir splat dosyası, kontrol noktası veya çalışma klasörü seçin"));

SS_MSG(viewer_open_another,
    EN("Open another"),
    JA("別のものを開く"),
    ZH_HANS("打开另一个"),
    ZH_HANT("開啟另一個"),
    KO("다른 것 열기"),
    DE("Weitere öffnen"),
    FR("En ouvrir un autre"),
    ES("Abrir otro"),
    PT("Abrir outro"),
    IT("Aprine un altro"),
    NL("Een andere openen"),
    RU("Открыть другую"),
    TR("Başka bir tane aç"));

SS_MSG(viewer_splat_count,
    EN("Splats: {0}   SH degree: {1}"),
    JA("スプラット数: {0}   SH次数: {1}"),
    ZH_HANS("泼溅数: {0}   球谐阶数: {1}"),
    ZH_HANT("潑濺數: {0}   球諧階數: {1}"),
    KO("스플랫 수: {0}   SH 차수: {1}"),
    DE("Splats: {0}   SH-Grad: {1}"),
    FR("Splats : {0}   Degré SH : {1}"),
    ES("Splats: {0}   Grado SH: {1}"),
    PT("Splats: {0}   Grau SH: {1}"),
    IT("Splat: {0}   Grado SH: {1}"),
    NL("Splats: {0}   SH-graad: {1}"),
    RU("Сплатов: {0}   Порядок SH: {1}"),
    TR("Splat: {0}   SH derecesi: {1}"));

SS_MSG(viewer_splat_count_sb,
    EN("Splats: {0}   SH degree: {1}   SB lobes: {2}"),
    JA("スプラット数: {0}   SH次数: {1}   SBローブ: {2}"),
    ZH_HANS("泼溅数: {0}   球谐阶数: {1}   SB 波瓣: {2}"),
    ZH_HANT("潑濺數: {0}   球諧階數: {1}   SB 波瓣: {2}"),
    KO("스플랫 수: {0}   SH 차수: {1}   SB 로브: {2}"),
    DE("Splats: {0}   SH-Grad: {1}   SB-Keulen: {2}"),
    FR("Splats : {0}   Degré SH : {1}   Lobes SB : {2}"),
    ES("Splats: {0}   Grado SH: {1}   Lóbulos SB: {2}"),
    PT("Splats: {0}   Grau SH: {1}   Lóbulos SB: {2}"),
    IT("Splat: {0}   Grado SH: {1}   Lobi SB: {2}"),
    NL("Splats: {0}   SH-graad: {1}   SB-lobben: {2}"),
    RU("Сплатов: {0}   Порядок SH: {1}   Лепестков SB: {2}"),
    TR("Splat: {0}   SH derecesi: {1}   SB lobu: {2}"));

SS_MSG(viewer_point_count,
    EN("Points: {0}"),
    JA("点の数: {0}"),
    ZH_HANS("点数: {0}"),
    ZH_HANT("點數: {0}"),
    KO("점 수: {0}"),
    DE("Punkte: {0}"),
    FR("Points : {0}"),
    ES("Puntos: {0}"),
    PT("Pontos: {0}"),
    IT("Punti: {0}"),
    NL("Punten: {0}"),
    RU("Точек: {0}"),
    TR("Nokta: {0}"));

SS_MSG(viewer_loading,
    EN("Loading the model..."),
    JA("モデルを読み込んでいます…"),
    ZH_HANS("正在载入模型…"),
    ZH_HANT("正在載入模型…"),
    KO("모델을 불러오는 중…"),
    DE("Modell wird geladen …"),
    FR("Chargement du modèle…"),
    ES("Cargando el modelo…"),
    PT("Carregando o modelo…"),
    IT("Caricamento del modello…"),
    NL("Model laden…"),
    RU("Загрузка модели…"),
    TR("Model yükleniyor…"));

SS_MSG(viewer_failed,
    EN("That file could not be opened."),
    JA("そのファイルは開けませんでした。"),
    ZH_HANS("无法打开该文件。"),
    ZH_HANT("無法開啟該檔案。"),
    KO("그 파일을 열 수 없었습니다."),
    DE("Diese Datei konnte nicht geöffnet werden."),
    FR("Ce fichier n'a pas pu être ouvert."),
    ES("No se pudo abrir ese archivo."),
    PT("Não foi possível abrir esse arquivo."),
    IT("Non è stato possibile aprire quel file."),
    NL("Dat bestand kon niet worden geopend."),
    RU("Не удалось открыть этот файл."),
    TR("Bu dosya açılamadı."));

SS_MSG(viewer_nothing_open,
    EN("Open a .ply file to see it here"),
    JA("PLYファイルを開くとここに表示されます"),
    ZH_HANS("打开一个 .ply 文件就会显示在这里"),
    ZH_HANT("開啟一個 .ply 檔案就會顯示在這裡"),
    KO(".ply 파일을 열면 여기에 표시됩니다"),
    DE("Eine .ply-Datei öffnen, um sie hier zu sehen"),
    FR("Ouvrez un fichier .ply pour le voir ici"),
    ES("Abra un archivo .ply para verlo aquí"),
    PT("Abra um arquivo .ply para vê-lo aqui"),
    IT("Apra un file .ply per vederlo qui"),
    NL("Open een .ply-bestand om het hier te zien"),
    RU("Откройте файл .ply, чтобы увидеть его здесь"),
    TR("Burada görmek için bir .ply dosyası açın"));

SS_MSG(viewer_reading,
    EN("Reading {0}"),
    JA("{0} を読み込んでいます"),
    ZH_HANS("正在读取 {0}"),
    ZH_HANT("正在讀取 {0}"),
    KO("{0} 읽는 중"),
    DE("{0} wird gelesen"),
    FR("Lecture de {0}"),
    ES("Leyendo {0}"),
    PT("Lendo {0}"),
    IT("Lettura di {0}"),
    NL("{0} lezen"),
    RU("Чтение {0}"),
    TR("{0} okunuyor"));

SS_MSG(viewer_loaded,
    EN("Loaded. Splats: {0}   SH degree: {1}"),
    JA("読み込みました。スプラット数: {0}   SH次数: {1}"),
    ZH_HANS("已载入。泼溅数: {0}   球谐阶数: {1}"),
    ZH_HANT("已載入。潑濺數: {0}   球諧階數: {1}"),
    KO("불러왔습니다. 스플랫 수: {0}   SH 차수: {1}"),
    DE("Geladen. Splats: {0}   SH-Grad: {1}"),
    FR("Chargé. Splats : {0}   Degré SH : {1}"),
    ES("Cargado. Splats: {0}   Grado SH: {1}"),
    PT("Carregado. Splats: {0}   Grau SH: {1}"),
    IT("Caricato. Splat: {0}   Grado SH: {1}"),
    NL("Geladen. Splats: {0}   SH-graad: {1}"),
    RU("Загружено. Сплатов: {0}   Порядок SH: {1}"),
    TR("Yüklendi. Splat: {0}   SH derecesi: {1}"));

SS_MSG(viewer_loaded_sb,
    EN("Loaded. Splats: {0}   SH degree: {1}   SB lobes: {2}"),
    JA("読み込みました。スプラット数: {0}   SH次数: {1}   SBローブ: {2}"),
    ZH_HANS("已载入。泼溅数: {0}   球谐阶数: {1}   SB 波瓣: {2}"),
    ZH_HANT("已載入。潑濺數: {0}   球諧階數: {1}   SB 波瓣: {2}"),
    KO("불러왔습니다. 스플랫 수: {0}   SH 차수: {1}   SB 로브: {2}"),
    DE("Geladen. Splats: {0}   SH-Grad: {1}   SB-Keulen: {2}"),
    FR("Chargé. Splats : {0}   Degré SH : {1}   Lobes SB : {2}"),
    ES("Cargado. Splats: {0}   Grado SH: {1}   Lóbulos SB: {2}"),
    PT("Carregado. Splats: {0}   Grau SH: {1}   Lóbulos SB: {2}"),
    IT("Caricato. Splat: {0}   Grado SH: {1}   Lobi SB: {2}"),
    NL("Geladen. Splats: {0}   SH-graad: {1}   SB-lobben: {2}"),
    RU("Загружено. Сплатов: {0}   Порядок SH: {1}   Лепестков SB: {2}"),
    TR("Yüklendi. Splat: {0}   SH derecesi: {1}   SB lobu: {2}"));

SS_MSG(viewer_loaded_points,
    EN("That file holds a point cloud, not Gaussians. Points: {0}"),
    JA("このファイルはガウシアンではなく点群です。点の数: {0}"),
    ZH_HANS("这个文件里是点云，不是高斯。点数: {0}"),
    ZH_HANT("這個檔案裡是點雲，不是高斯。點數: {0}"),
    KO("이 파일에는 가우시안이 아니라 점군이 들어 있습니다. 점 수: {0}"),
    DE("Diese Datei enthält eine Punktwolke, keine Gauß-Verteilungen. Punkte: {0}"),
    FR("Ce fichier contient un nuage de points, pas des gaussiennes. Points : {0}"),
    ES("Ese archivo contiene una nube de puntos, no gaussianas. Puntos: {0}"),
    PT("Esse arquivo contém uma nuvem de pontos, não gaussianas. Pontos: {0}"),
    IT("Quel file contiene una nuvola di punti, non gaussiane. Punti: {0}"),
    NL("Dat bestand bevat een puntenwolk, geen Gaussianen. Punten: {0}"),
    RU("В этом файле облако точек, а не гауссианы. Точек: {0}"),
    TR("Bu dosyada gauss'lar değil, bir nokta bulutu var. Nokta: {0}"));

SS_MSG(viewer_not_a_splat_file,
    EN("{0} is a PLY file, but it holds neither Gaussians nor points."),
    JA("{0} はPLYファイルですが、ガウシアンも点も入っていません。"),
    ZH_HANS("{0} 是 PLY 文件，但里面既没有高斯也没有点。"),
    ZH_HANT("{0} 是 PLY 檔案，但裡面既沒有高斯也沒有點。"),
    KO("{0} 은(는) PLY 파일이지만 가우시안도 점도 들어 있지 않습니다."),
    DE("{0} ist eine PLY-Datei, enthält aber weder Gauß-Verteilungen noch Punkte."),
    FR("{0} est un fichier PLY, mais il ne contient ni gaussiennes ni points."),
    ES("{0} es un archivo PLY, pero no contiene ni gaussianas ni puntos."),
    PT("{0} é um arquivo PLY, mas não contém gaussianas nem pontos."),
    IT("{0} è un file PLY, ma non contiene né gaussiane né punti."),
    NL("{0} is een PLY-bestand, maar bevat Gaussianen noch punten."),
    RU("{0} — файл PLY, но в нём нет ни гауссиан, ни точек."),
    TR("{0} bir PLY dosyası, ama içinde ne gauss ne de nokta var."));

SS_MSG(viewer_no_splats,
    EN("That file has no Gaussians in it."),
    JA("このファイルにはガウシアンが入っていません。"),
    ZH_HANS("这个文件里没有高斯。"),
    ZH_HANT("這個檔案裡沒有高斯。"),
    KO("이 파일에는 가우시안이 없습니다."),
    DE("Diese Datei enthält keine Gauß-Verteilungen."),
    FR("Ce fichier ne contient aucune gaussienne."),
    ES("Ese archivo no contiene ninguna gaussiana."),
    PT("Esse arquivo não contém nenhuma gaussiana."),
    IT("Quel file non contiene alcuna gaussiana."),
    NL("Dat bestand bevat geen Gaussianen."),
    RU("В этом файле нет гауссиан."),
    TR("Bu dosyada hiç gauss yok."));

SS_MSG(viewer_using_run_config,
    EN("Using the run's own settings from {0}"),
    JA("実行時の設定を {0} から読み込みました"),
    ZH_HANS("使用来自 {0} 的该次运行的设置"),
    ZH_HANT("使用來自 {0} 的該次執行的設定"),
    KO("{0} 에 있는 해당 실행의 설정을 사용합니다"),
    DE("Die Einstellungen des Laufs aus {0} werden verwendet"),
    FR("Utilisation des réglages de l'exécution issus de {0}"),
    ES("Se usan los ajustes de la ejecución tomados de {0}"),
    PT("Usando as configurações da execução vindas de {0}"),
    IT("Si usano le impostazioni dell'esecuzione da {0}"),
    NL("De instellingen van de run uit {0} worden gebruikt"),
    RU("Используются настройки запуска из {0}"),
    TR("Çalışmanın {0} içindeki kendi ayarları kullanılıyor"));

SS_MSG(viewer_run_config_unreadable,
    EN("The run's settings could not be read ({0}); using the defaults."),
    JA("実行時の設定を読み取れませんでした（{0}）。既定値を使います。"),
    ZH_HANS("无法读取该次运行的设置（{0}），改用默认值。"),
    ZH_HANT("無法讀取該次執行的設定（{0}），改用預設值。"),
    KO("실행 설정을 읽지 못했습니다({0}). 기본값을 사용합니다."),
    DE("Die Einstellungen des Laufs konnten nicht gelesen werden ({0}); es "
       "gelten die Standardwerte."),
    FR("Les réglages de l'exécution n'ont pas pu être lus ({0}) ; les valeurs "
       "par défaut sont utilisées."),
    ES("No se pudieron leer los ajustes de la ejecución ({0}); se usan los "
       "valores predeterminados."),
    PT("Não foi possível ler as configurações da execução ({0}); usando os "
       "valores padrão."),
    IT("Non è stato possibile leggere le impostazioni dell'esecuzione ({0}); "
       "si usano i valori predefiniti."),
    NL("De instellingen van de run konden niet worden gelezen ({0}); de "
       "standaardwaarden worden gebruikt."),
    RU("Не удалось прочитать настройки запуска ({0}); используются значения по "
       "умолчанию."),
    TR("Çalışmanın ayarları okunamadı ({0}); varsayılanlar kullanılıyor."));

SS_MSG(compare_add_model,
    EN("Add a model"),
    JA("モデルを追加"),
    ZH_HANS("添加模型"),
    ZH_HANT("新增模型"),
    KO("모델 추가"),
    DE("Modell hinzufügen"),
    FR("Ajouter un modèle"),
    ES("Añadir un modelo"),
    PT("Adicionar um modelo"),
    IT("Aggiungi un modello"),
    NL("Een model toevoegen"),
    RU("Добавить модель"),
    TR("Model ekle"));

SS_MSG(compare_add_model_help,
    EN("Show another model beside this one. Up to four are shown at once, on "
       "one camera, so what differs between the panes is the model."),
    JA("このモデルの隣にもう一つ表示します。最大4つまで、同じカメラで並べて"
       "表示されるので、ペインごとの違いはモデルそのものだけになります。"),
    ZH_HANS("在这个模型旁边再显示一个。最多同时显示四个，共用同一台相机，"
            "所以各窗格之间的差别只在于模型本身。"),
    ZH_HANT("在這個模型旁邊再顯示一個。最多同時顯示四個，共用同一台相機，"
            "所以各窗格之間的差別只在於模型本身。"),
    KO("이 모델 옆에 다른 모델을 함께 봅니다. 한 번에 네 개까지, 같은 카메라로 "
       "보여 주므로 창마다 다른 것은 모델뿐입니다."),
    DE("Ein weiteres Modell daneben zeigen. Bis zu vier auf einmal, mit einer "
       "gemeinsamen Kamera, sodass sich die Bereiche nur im Modell "
       "unterscheiden."),
    FR("Afficher un autre modèle à côté de celui-ci. Jusqu'à quatre à la fois, "
       "sur une seule caméra, de sorte que seule la différence de modèle "
       "sépare les volets."),
    ES("Mostrar otro modelo junto a este. Hasta cuatro a la vez, con una sola "
       "cámara, de modo que lo único que cambia entre los paneles es el "
       "modelo."),
    PT("Mostrar outro modelo ao lado deste. Até quatro de uma vez, com uma só "
       "câmera, de modo que o que muda entre os painéis é o modelo."),
    IT("Mostri un altro modello accanto a questo. Fino a quattro insieme, con "
       "una sola telecamera, così tra i riquadri cambia solo il modello."),
    NL("Nog een model ernaast tonen. Maximaal vier tegelijk, met één camera, "
       "zodat alleen het model tussen de vensters verschilt."),
    RU("Показать рядом ещё одну модель. До четырёх сразу, с одной камерой, так "
       "что панели различаются только моделью."),
    TR("Bunun yanında başka bir model gösterin. Aynı anda dörde kadar, tek bir "
       "kamerayla; böylece bölmeler arasındaki tek fark modeldir."));

SS_MSG(compare_full,
    EN("Four models is as many as fit."),
    JA("表示できるモデルは4つまでです。"),
    ZH_HANS("最多只能同时显示四个模型。"),
    ZH_HANT("最多只能同時顯示四個模型。"),
    KO("모델은 네 개까지만 들어갑니다."),
    DE("Mehr als vier Modelle passen nicht."),
    FR("Quatre modèles, c'est tout ce qui tient."),
    ES("Caben cuatro modelos como máximo."),
    PT("Cabem no máximo quatro modelos."),
    IT("Più di quattro modelli non ci stanno."),
    NL("Meer dan vier modellen passen er niet."),
    RU("Больше четырёх моделей не помещается."),
    TR("Dörtten fazla model sığmıyor."));

SS_MSG(compare_from_file,
    EN("From a file..."),
    JA("ファイルから…"),
    ZH_HANS("从文件…"),
    ZH_HANT("從檔案…"),
    KO("파일에서…"),
    DE("Aus einer Datei …"),
    FR("Depuis un fichier…"),
    ES("Desde un archivo…"),
    PT("De um arquivo…"),
    IT("Da un file…"),
    NL("Uit een bestand…"),
    RU("Из файла…"),
    TR("Bir dosyadan…"));

SS_MSG(compare_link_views,
    EN("Move all views together"),
    JA("すべてのビューを一緒に動かす"),
    ZH_HANS("所有视图一起转"),
    ZH_HANT("所有檢視一起轉"),
    KO("모든 화면을 함께 움직이기"),
    DE("Alle Ansichten zusammen bewegen"),
    FR("Déplacer toutes les vues ensemble"),
    ES("Mover todas las vistas juntas"),
    PT("Mover todas as vistas juntas"),
    IT("Muovi tutte le viste insieme"),
    NL("Alle beelden samen bewegen"),
    RU("Двигать все виды вместе"),
    TR("Tüm görünümleri birlikte oynat"));

SS_MSG(compare_link_views_help,
    EN("One camera for every pane, so the same part of the scene is on screen "
       "in all of them at once."),
    JA("すべてのペインが同じカメラを使うので、シーンの同じ場所が同時に映ります。"),
    ZH_HANS("所有窗格共用一台相机，因此同一处场景会同时出现在每个窗格里。"),
    ZH_HANT("所有窗格共用一台相機，因此同一處場景會同時出現在每個窗格裡。"),
    KO("모든 창이 카메라를 공유하므로 장면의 같은 부분이 동시에 보입니다."),
    DE("Eine Kamera für alle Bereiche, sodass überall gleichzeitig dieselbe "
       "Stelle der Szene zu sehen ist."),
    FR("Une seule caméra pour tous les volets, si bien que le même endroit de "
       "la scène est visible partout en même temps."),
    ES("Una sola cámara para todos los paneles, de modo que en todos se vea a "
       "la vez la misma parte de la escena."),
    PT("Uma só câmera para todos os painéis, de modo que a mesma parte da cena "
       "apareça em todos ao mesmo tempo."),
    IT("Una sola telecamera per tutti i riquadri, così in ciascuno si vede "
       "insieme lo stesso punto della scena."),
    NL("Eén camera voor alle vensters, zodat overal tegelijk hetzelfde deel "
       "van de scène te zien is."),
    RU("Одна камера на все панели, поэтому во всех сразу видно одно и то же "
       "место сцены."),
    TR("Tüm bölmeler için tek bir kamera; böylece sahnenin aynı yeri hepsinde "
       "aynı anda görünür."));

SS_MSG(compare_wipe,
    EN("Wipe"),
    JA("ワイプ"),
    ZH_HANS("划像对比"),
    ZH_HANT("劃像對比"),
    KO("와이프"),
    DE("Wischen"),
    FR("Volet"),
    ES("Cortinilla"),
    PT("Cortina"),
    IT("Tendina"),
    NL("Veegbalk"),
    RU("Шторка"),
    TR("Perde"));

SS_MSG(compare_wipe_help,
    EN("Show all open models in one pane with one camera. Drag the dividers to "
       "adjust each model's section."),
    JA("開いているすべてのモデルを同じカメラで1つのペインに表示します。"
       "境界線をドラッグして各モデルの表示範囲を調整できます。"),
    ZH_HANS("在一个窗格中用同一台相机显示所有已打开的模型。"
            "拖动分界线可调整各模型的显示范围。"),
    ZH_HANT("在一個窗格中用同一台相機顯示所有已開啟的模型。"
            "拖動分界線可調整各模型的顯示範圍。"),
    KO("열린 모든 모델을 같은 카메라로 한 창에 표시합니다. "
       "경계선을 끌어서 각 모델의 표시 영역을 조절합니다."),
    DE("Alle geöffneten Modelle in einem Bereich mit einer Kamera anzeigen. "
       "Die Trennlinien ziehen, um den Bereich jedes Modells anzupassen."),
    FR("Afficher tous les modèles ouverts dans un volet avec une seule caméra. "
       "Faites glisser les séparateurs pour ajuster la zone de chaque modèle."),
    ES("Muestra todos los modelos abiertos en un panel con una cámara. "
       "Arrastra los separadores para ajustar la sección de cada modelo."),
    PT("Mostra todos os modelos abertos num painel com uma câmera. "
       "Arraste as divisórias para ajustar a seção de cada modelo."),
    IT("Mostra tutti i modelli aperti in un riquadro con una telecamera. "
       "Trascina i divisori per regolare la sezione di ogni modello."),
    NL("Toon alle geopende modellen in één venster met één camera. "
       "Sleep de scheidingslijnen om het deel van elk model aan te passen."),
    RU("Все открытые модели в одной панели с одной камерой. "
       "Перетаскивайте разделители, чтобы настроить область каждой модели."),
    TR("Açık tüm modelleri tek bölmede, tek kamerayla gösterin. "
       "Her modelin alanını ayarlamak için ayırıcıları sürükleyin."));

SS_MSG(compare_placement_help,
    EN("Where this model sits, and where its pane is."),
    JA("このモデルの置き場所と、ペインの並び順です。"),
    ZH_HANS("这个模型摆在哪里，以及它的窗格排在第几个。"),
    ZH_HANT("這個模型擺在哪裡，以及它的窗格排在第幾個。"),
    KO("이 모델을 어디에 놓을지와, 창이 몇 번째인지입니다."),
    DE("Wo dieses Modell steht und an welcher Stelle sein Bereich liegt."),
    FR("Où se place ce modèle, et à quel rang se trouve son volet."),
    ES("Dónde se coloca este modelo y en qué lugar queda su panel."),
    PT("Onde este modelo fica e em que lugar está o painel dele."),
    IT("Dove sta questo modello e in che posizione sta il suo riquadro."),
    NL("Waar dit model staat en op welke plek zijn venster zit."),
    RU("Где стоит эта модель и на каком месте её панель."),
    TR("Bu modelin nerede durduğu ve bölmesinin kaçıncı sırada olduğu."));

SS_MSG(compare_align_first,
    EN("Place in the first model's frame"),
    JA("最初のモデルの座標に合わせる"),
    ZH_HANS("放到第一个模型的坐标里"),
    ZH_HANT("放到第一個模型的座標裡"),
    KO("첫 모델의 좌표에 맞추기"),
    DE("In den Rahmen des ersten Modells setzen"),
    FR("Placer dans le repère du premier modèle"),
    ES("Colocar en el marco del primer modelo"),
    PT("Colocar no referencial do primeiro modelo"),
    IT("Colloca nel sistema del primo modello"),
    NL("In het assenstelsel van het eerste model zetten"),
    RU("Поместить в систему координат первой модели"),
    TR("İlk modelin çerçevesine yerleştir"));

SS_MSG(compare_align_first_help,
    EN("Two reconstructions of one scene share coordinates, so this lines them up exactly.\nTurn it off for a model that has nothing to do with the first one, and it is framed on its own instead."),
    JA("同じシーンを再構成した2つのモデルは座標を共有するので、これでぴったり重なります。最初のモデルと無関係なモデルではオフにすると、それ自体に合わせて表示されます。"),
    ZH_HANS("同一场景的两次重建共用坐标，打开它就能精确对齐。若这个模型与第一个毫无关系，请关掉它，模型会按自身范围取景。"),
    ZH_HANT("同一場景的兩次重建共用座標，開啟它就能精確對齊。若這個模型與第一個毫無關係，請關掉它，模型會依自身範圍取景。"),
    KO("같은 장면을 두 번 복원한 모델은 좌표가 같으므로 이것으로 정확히 겹칩니다. 첫 모델과 무관한 모델이라면 꺼 두세요. 그러면 그 모델 자체에 맞춰 보여 줍니다."),
    DE("Zwei Rekonstruktionen einer Szene teilen sich die Koordinaten, das bringt sie genau zur Deckung.\nFür ein Modell, das mit dem ersten nichts zu tun hat, ausschalten -- dann wird es für sich gerahmt."),
    FR("Deux reconstructions d'une même scène partagent leurs coordonnées : cela les superpose exactement.\nDésactivez-le pour un modèle sans rapport avec le premier, il sera alors cadré pour lui-même."),
    ES("Dos reconstrucciones de una misma escena comparten coordenadas, así que esto las superpone con exactitud.\nDesactívelo para un modelo que no tenga que ver con el primero: se encuadrará por sí solo."),
    PT("Duas reconstruções de uma mesma cena partilham coordenadas, então isto as sobrepõe com exatidão.\nDesligue para um modelo que nada tenha a ver com o primeiro: ele será enquadrado por si só."),
    IT("Due ricostruzioni della stessa scena condividono le coordinate, quindi così si sovrappongono esattamente.\nLo disattivi per un modello che con il primo non c'entra: verrà inquadrato per conto suo."),
    NL("Twee reconstructies van dezelfde scène delen coördinaten, dus hiermee vallen ze precies samen.\nZet het uit voor een model dat niets met het eerste te maken heeft; dat wordt dan op zichzelf ingekaderd."),
    RU("Две реконструкции одной сцены имеют общие координаты, поэтому так они совпадут точно. Для модели, не связанной с первой, выключите -- она будет вписана сама по себе."),
    TR("Aynı sahnenin iki yeniden oluşturması koordinatları paylaşır, bu da onları tam olarak üst üste getirir.\nİlkiyle ilgisi olmayan bir model için kapatın; o zaman kendi başına çerçevelenir."));

SS_MSG(compare_position,
    EN("Position"),
    JA("位置"),
    ZH_HANS("位置"),
    ZH_HANT("位置"),
    KO("위치"),
    DE("Position"),
    FR("Position"),
    ES("Posición"),
    PT("Posição"),
    IT("Posizione"),
    NL("Positie"),
    RU("Положение"),
    TR("Konum"));

SS_MSG(compare_rotation,
    EN("Rotation"),
    JA("回転"),
    ZH_HANS("旋转"),
    ZH_HANT("旋轉"),
    KO("회전"),
    DE("Drehung"),
    FR("Rotation"),
    ES("Rotación"),
    PT("Rotação"),
    IT("Rotazione"),
    NL("Draaiing"),
    RU("Поворот"),
    TR("Döndürme"));

SS_MSG(compare_size,
    EN("Size"),
    JA("大きさ"),
    ZH_HANS("大小"),
    ZH_HANT("大小"),
    KO("크기"),
    DE("Größe"),
    FR("Taille"),
    ES("Tamaño"),
    PT("Tamanho"),
    IT("Dimensione"),
    NL("Grootte"),
    RU("Размер"),
    TR("Boyut"));

SS_MSG(compare_reset_placement,
    EN("Put it back"),
    JA("元に戻す"),
    ZH_HANS("放回原处"),
    ZH_HANT("放回原處"),
    KO("원래대로"),
    DE("Zurücksetzen"),
    FR("Remettre en place"),
    ES("Devolver a su sitio"),
    PT("Voltar ao lugar"),
    IT("Rimetti a posto"),
    NL("Terugzetten"),
    RU("Вернуть на место"),
    TR("Yerine geri koy"));

SS_MSG(compare_move_left,
    EN("Move left"),
    JA("左へ"),
    ZH_HANS("左移"),
    ZH_HANT("左移"),
    KO("왼쪽으로"),
    DE("Nach links"),
    FR("Vers la gauche"),
    ES("A la izquierda"),
    PT("Para a esquerda"),
    IT("A sinistra"),
    NL("Naar links"),
    RU("Влево"),
    TR("Sola al"));

SS_MSG(compare_move_right,
    EN("Move right"),
    JA("右へ"),
    ZH_HANS("右移"),
    ZH_HANT("右移"),
    KO("오른쪽으로"),
    DE("Nach rechts"),
    FR("Vers la droite"),
    ES("A la derecha"),
    PT("Para a direita"),
    IT("A destra"),
    NL("Naar rechts"),
    RU("Вправо"),
    TR("Sağa al"));

SS_MSG(compare_remove,
    EN("Close this one"),
    JA("これを閉じる"),
    ZH_HANS("关掉这个"),
    ZH_HANT("關掉這個"),
    KO("이것 닫기"),
    DE("Dieses schließen"),
    FR("Fermer celui-ci"),
    ES("Cerrar este"),
    PT("Fechar este"),
    IT("Chiudi questo"),
    NL("Deze sluiten"),
    RU("Закрыть эту"),
    TR("Bunu kapat"));

SS_MSG(confirm_open_splat,
    EN("Stop training and open the model file?"),
    JA("学習を停止してモデルファイルを開きますか？"),
    ZH_HANS("停止训练并打开该模型文件吗？"),
    ZH_HANT("停止訓練並開啟該模型檔案嗎？"),
    KO("학습을 멈추고 모델 파일을 열까요?"),
    DE("Training anhalten und die Modelldatei öffnen?"),
    FR("Arrêter l'entraînement et ouvrir le fichier de modèle ?"),
    ES("¿Detener el entrenamiento y abrir el archivo de modelo?"),
    PT("Parar o treinamento e abrir o arquivo de modelo?"),
    IT("Fermare l'addestramento e aprire il file del modello?"),
    NL("Training stoppen en het modelbestand openen?"),
    RU("Остановить обучение и открыть файл модели?"),
    TR("Eğitimi durdurup model dosyasını açalım mı?"));


// ===========================================================================
// Saved presets
//
// The built-in presets are code and their labels are translated in
// i18n/catalog/Train.h. These are the words around the ones the user saves:
// the save dialog, the picker's two groups, and what comes back afterwards.
// ===========================================================================

SS_MSG(preset_builtin_group,
    EN("Built-in"),      JA("組み込み"),       ZH_HANS("内置"),     ZH_HANT("內建"),
    KO("기본 제공"),      DE("Mitgeliefert"), FR("Fournis"),      ES("Incluidos"),
    PT("Incluídas"),     IT("Incluse"),      NL("Ingebouwd"),    RU("Встроенные"),
    TR("Yerleşik"));
SS_MSG(preset_user_group,
    EN("Saved"),         JA("保存済み"),       ZH_HANS("已保存"),   ZH_HANT("已儲存"),
    KO("저장됨"),         DE("Gespeichert"),  FR("Enregistrés"),  ES("Guardados"),
    PT("Salvas"),        IT("Salvate"),      NL("Opgeslagen"),   RU("Сохранённые"),
    TR("Kaydedilmiş"));
SS_MSG(preset_none_saved,
    EN("Nothing saved yet"),
    JA("まだ何も保存されていません"),
    ZH_HANS("还没有保存任何内容"),
    ZH_HANT("還沒有儲存任何內容"),
    KO("아직 저장한 것이 없습니다"),
    DE("Noch nichts gespeichert"),
    FR("Rien d'enregistré pour l'instant"),
    ES("Todavía no hay nada guardado"),
    PT("Ainda não há nada salvo"),
    IT("Non c'è ancora niente di salvato"),
    NL("Nog niets opgeslagen"),
    RU("Пока ничего не сохранено"),
    TR("Henüz kaydedilmiş bir şey yok"));
SS_MSG(preset_save,
    EN("Save as preset..."),
    JA("プリセットとして保存…"),
    ZH_HANS("保存为预设…"),
    ZH_HANT("儲存為預設…"),
    KO("프리셋으로 저장…"),
    DE("Als Voreinstellung speichern …"),
    FR("Enregistrer comme préréglage…"),
    ES("Guardar como ajuste…"),
    PT("Salvar como predefinição…"),
    IT("Salva come preimpostazione…"),
    NL("Opslaan als voorinstelling…"),
    RU("Сохранить как пресет…"),
    TR("Hazır ayar olarak kaydet…"));
SS_MSG(preset_save_help,
    EN("Saves these options for another dataset or batch run. Dataset and output paths are excluded."),
    JA("この設定を別のデータセットやバッチ実行用に保存します。データセットと出力先のパスは含みません。"),
    ZH_HANS("保存当前选项，供其他数据集或批量任务使用。不保存数据集和输出路径。"),
    ZH_HANT("儲存目前選項，供其他資料集或批次任務使用。不儲存資料集與輸出路徑。"),
    KO("다른 데이터셋이나 일괄 실행에 쓸 옵션을 저장합니다. 데이터셋과 출력 경로는 제외됩니다."),
    DE("Speichert diese Optionen für andere Datensätze oder Stapelläufe. Datensatz- und Ausgabepfade sind ausgenommen."),
    FR("Enregistre ces options pour un autre jeu de données ou un lot. Les chemins des données et de sortie sont exclus."),
    ES("Guarda estas opciones para otro conjunto de datos o lote. Excluye las rutas de datos y salida."),
    PT("Guarda estas opções para outro conjunto de dados ou lote. Exclui os caminhos dos dados e da saída."),
    IT("Salva queste opzioni per altri dati o esecuzioni in batch. Esclude i percorsi dei dati e di output."),
    NL("Bewaart deze opties voor een andere dataset of batch. Dataset- en uitvoerpaden worden niet opgeslagen."),
    RU("Сохраняет настройки для другого набора данных или пакетного запуска. Пути к данным и результатам не включаются."),
    TR("Bu seçenekleri başka bir veri kümesi veya toplu çalışma için kaydeder. Veri ve çıktı yolları dahil edilmez."));
SS_MSG(preset_load,
    EN("Load preset..."),
    JA("プリセットを読み込む…"),
    ZH_HANS("加载预设…"),
    ZH_HANT("載入預設…"),
    KO("프리셋 불러오기…"),
    DE("Voreinstellung laden …"),
    FR("Charger un préréglage…"),
    ES("Cargar un ajuste…"),
    PT("Carregar predefinição…"),
    IT("Carica preimpostazione…"),
    NL("Voorinstelling laden…"),
    RU("Загрузить пресет…"),
    TR("Hazır ayar yükle…"));
SS_MSG(preset_load_help,
    EN("Loads a preset or a run’s config.json. Missing options use defaults; options unsupported by this build are ignored."),
    JA("プリセットまたは実行の config.json を読み込みます。未指定の項目は既定値を使い、このビルドにない項目は無視します。"),
    ZH_HANS("加载预设或运行的 config.json。缺失选项使用默认值；本版本不支持的选项会被忽略。"),
    ZH_HANT("載入預設或執行的 config.json。缺少的選項使用預設值；本版本不支援的選項會被忽略。"),
    KO("프리셋이나 실행의 config.json을 불러옵니다. 없는 옵션은 기본값을 쓰며 이 빌드가 지원하지 않는 옵션은 무시합니다."),
    DE("Lädt ein Preset oder die config.json eines Laufs. Fehlende Optionen nutzen Vorgaben; hier nicht unterstützte Optionen werden ignoriert."),
    FR("Charge un préréglage ou le config.json d’une exécution. Options absentes : valeurs par défaut ; options non prises en charge : ignorées."),
    ES("Carga un preajuste o el config.json de una ejecución. Las opciones ausentes usan valores por defecto; las no compatibles se ignoran."),
    PT("Carrega uma predefinição ou o config.json de uma execução. Opções ausentes usam valores padrão; as não suportadas são ignoradas."),
    IT("Carica un preset o il config.json di una sessione. Le opzioni assenti usano i valori predefiniti; quelle non supportate sono ignorate."),
    NL("Laadt een preset of config.json van een run. Ontbrekende opties krijgen standaardwaarden; niet-ondersteunde opties worden genegeerd."),
    RU("Загружает пресет или config.json запуска. Пропущенные параметры получают значения по умолчанию; неподдерживаемые игнорируются."),
    TR("Bir önayar veya çalışmanın config.json dosyasını yükler. Eksik seçenekler varsayılanı kullanır; desteklenmeyenler yok sayılır."));
SS_MSG(preset_drop_hint,
    EN("Tip: a preset (or a run's config.json) can be dropped onto this window."),
    JA("ヒント: プリセット（や実行の config.json）はこのウィンドウにドラッグ＆"
       "ドロップできます。"),
    ZH_HANS("提示：预设文件（或某次运行的 config.json）可以直接拖到这个窗口上。"),
    ZH_HANT("提示：預設檔（或某次執行的 config.json）可以直接拖到這個視窗上。"),
    KO("팁: 프리셋(또는 어떤 실행의 config.json)을 이 창에 끌어다 놓을 수 있습니다."),
    DE("Tipp: Eine Voreinstellung (oder die config.json eines Laufs) lässt sich "
       "auf dieses Fenster ziehen."),
    FR("Astuce : un préréglage (ou le config.json d'une exécution) peut être "
       "déposé sur cette fenêtre."),
    ES("Sugerencia: puedes arrastrar un ajuste (o el config.json de una "
       "ejecución) hasta esta ventana."),
    PT("Dica: dá para arrastar uma predefinição (ou o config.json de uma "
       "execução) até esta janela."),
    IT("Suggerimento: una preimpostazione (o il config.json di un'esecuzione) "
       "si può trascinare su questa finestra."),
    NL("Tip: een voorinstelling (of de config.json van een run) kun je op dit "
       "venster slepen."),
    RU("Подсказка: пресет (или config.json запуска) можно перетащить в это окно."),
    TR("İpucu: bir hazır ayarı (ya da bir çalıştırmanın config.json dosyasını) "
       "bu pencereye sürükleyip bırakabilirsiniz."));

SS_MSG(preset_save_title,
    EN("Save Preset"),
    JA("プリセットを保存"),
    ZH_HANS("保存预设"),
    ZH_HANT("儲存預設"),
    KO("프리셋 저장"),
    DE("Voreinstellung speichern"),
    FR("Enregistrer le préréglage"),
    ES("Guardar ajuste"),
    PT("Salvar predefinição"),
    IT("Salva preimpostazione"),
    NL("Voorinstelling opslaan"),
    RU("Сохранить пресет"),
    TR("Hazır ayarı kaydet"));
SS_MSG(preset_name,
    EN("Name"),          JA("名前"),          ZH_HANS("名称"),     ZH_HANT("名稱"),
    KO("이름"),           DE("Name"),         FR("Nom"),          ES("Nombre"),
    PT("Nome"),          IT("Nome"),         NL("Naam"),         RU("Название"),
    TR("Ad"));
SS_MSG(preset_name_hint,
    EN("e.g. Indoor handheld, high detail"),
    JA("例: 屋内・手持ち・高精細"),
    ZH_HANS("例如：室内手持，高细节"),
    ZH_HANT("例如：室內手持，高細節"),
    KO("예: 실내 손각대, 고디테일"),
    DE("z. B. Innen, aus der Hand, hohes Detail"),
    FR("p. ex. Intérieur à main levée, très détaillé"),
    ES("p. ej. Interior a mano alzada, mucho detalle"),
    PT("por ex. Interior na mão, muito detalhe"),
    IT("es. Interni a mano libera, alto dettaglio"),
    NL("bijv. Binnen uit de hand, veel detail"),
    RU("напр. В помещении с рук, высокая детализация"),
    TR("örn. İç mekân elde, yüksek ayrıntı"));
SS_MSG(preset_desc,
    EN("Description"),   JA("説明"),          ZH_HANS("说明"),     ZH_HANT("說明"),
    KO("설명"),           DE("Beschreibung"), FR("Description"),  ES("Descripción"),
    PT("Descrição"),     IT("Descrizione"),  NL("Beschrijving"), RU("Описание"),
    TR("Açıklama"));
SS_MSG(preset_desc_hint,
    EN("What this preset is for (optional)"),
    JA("このプリセットの用途（任意）"),
    ZH_HANS("这个预设的用途（可选）"),
    ZH_HANT("這個預設的用途（選填）"),
    KO("이 프리셋의 용도(선택)"),
    DE("Wofür diese Voreinstellung gedacht ist (optional)"),
    FR("À quoi sert ce préréglage (facultatif)"),
    ES("Para qué sirve este ajuste (opcional)"),
    PT("Para que serve esta predefinição (opcional)"),
    IT("A cosa serve questa preimpostazione (facoltativo)"),
    NL("Waar deze voorinstelling voor is (optioneel)"),
    RU("Для чего этот пресет (необязательно)"),
    TR("Bu hazır ayar ne için (isteğe bağlı)"));
SS_MSG(preset_path_label,
    EN("File"),          JA("ファイル"),       ZH_HANS("文件"),     ZH_HANT("檔案"),
    KO("파일"),           DE("Datei"),        FR("Fichier"),      ES("Archivo"),
    PT("Arquivo"),       IT("File"),         NL("Bestand"),      RU("Файл"),
    TR("Dosya"));
SS_MSG(preset_path_help,
    EN("Where the preset is written. Presets in the default folder are offered "
       "in the picker above; one saved anywhere else is loaded by path."),
    JA("プリセットの書き出し先です。既定のフォルダーにあるものは上の一覧に"
       "並びます。それ以外の場所に保存したものは、パスを指定して読み込みます。"),
    ZH_HANS("预设写到哪里。放在默认文件夹里的预设会出现在上面的下拉列表中；"
            "存到别处的则要按路径加载。"),
    ZH_HANT("預設寫到哪裡。放在預設資料夾裡的預設會出現在上面的下拉清單中；"
            "存到別處的則要按路徑載入。"),
    KO("프리셋을 어디에 쓸지입니다. 기본 폴더에 있는 프리셋은 위 목록에 나오고, "
       "다른 곳에 저장한 것은 경로로 불러옵니다."),
    DE("Wohin die Voreinstellung geschrieben wird. Voreinstellungen im "
       "Standardordner erscheinen oben in der Auswahl; anderswo gespeicherte "
       "werden über ihren Pfad geladen."),
    FR("Où le préréglage est écrit. Ceux du dossier par défaut apparaissent "
       "dans la liste ci-dessus ; un préréglage enregistré ailleurs se charge "
       "par son chemin."),
    ES("Dónde se escribe el ajuste. Los del carpeta predeterminada aparecen en "
       "la lista de arriba; uno guardado en otro sitio se carga por su ruta."),
    PT("Onde a predefinição é escrita. As da pasta padrão aparecem na lista "
       "acima; uma salva em outro lugar é carregada pelo caminho."),
    IT("Dove viene scritta la preimpostazione. Quelle nella cartella "
       "predefinita compaiono nell'elenco qui sopra; una salvata altrove si "
       "carica indicandone il percorso."),
    NL("Waar de voorinstelling wordt weggeschreven. Voorinstellingen in de "
       "standaardmap staan in de lijst hierboven; eentje die elders is "
       "opgeslagen laad je via het pad."),
    RU("Куда записывается пресет. Пресеты из папки по умолчанию появляются в "
       "списке выше; сохранённый в другом месте загружается по пути."),
    TR("Hazır ayarın nereye yazılacağı. Varsayılan klasördekiler yukarıdaki "
       "listede görünür; başka bir yere kaydedilen, yolu verilerek yüklenir."));
SS_MSG(preset_use_default_folder,
    EN("Default folder"),
    JA("既定のフォルダー"),
    ZH_HANS("默认文件夹"),
    ZH_HANT("預設資料夾"),
    KO("기본 폴더"),
    DE("Standardordner"),
    FR("Dossier par défaut"),
    ES("Carpeta predeterminada"),
    PT("Pasta padrão"),
    IT("Cartella predefinita"),
    NL("Standaardmap"),
    RU("Папка по умолчанию"),
    TR("Varsayılan klasör"));
SS_MSG(preset_name_required,
    EN("Give the preset a name first."),
    JA("先にプリセットの名前を入れてください。"),
    ZH_HANS("请先给这个预设起个名字。"),
    ZH_HANT("請先給這個預設取個名稱。"),
    KO("먼저 프리셋 이름을 지어 주세요."),
    DE("Bitte zuerst einen Namen für die Voreinstellung eingeben."),
    FR("Donnez d'abord un nom au préréglage."),
    ES("Primero dale un nombre al ajuste."),
    PT("Primeiro dê um nome à predefinição."),
    IT("Prima dai un nome alla preimpostazione."),
    NL("Geef de voorinstelling eerst een naam."),
    RU("Сначала задайте название пресета."),
    TR("Önce hazır ayara bir ad verin."));
SS_MSG(preset_overwrite_warn,
    EN("A file already exists here and will be replaced."),
    JA("ここには既にファイルがあり、上書きされます。"),
    ZH_HANS("这里已经有一个文件，将被替换。"),
    ZH_HANT("這裡已經有一個檔案，將被取代。"),
    KO("여기에 이미 파일이 있어 덮어씁니다."),
    DE("Hier gibt es bereits eine Datei; sie wird ersetzt."),
    FR("Un fichier existe déjà ici et sera remplacé."),
    ES("Aquí ya hay un archivo y se reemplazará."),
    PT("Já existe um arquivo aqui e ele será substituído."),
    IT("Qui c'è già un file e verrà sostituito."),
    NL("Hier staat al een bestand; dat wordt vervangen."),
    RU("Здесь уже есть файл, он будет заменён."),
    TR("Burada zaten bir dosya var ve değiştirilecek."));
SS_MSG(preset_save_button,
    EN("Save"),          JA("保存"),          ZH_HANS("保存"),     ZH_HANT("儲存"),
    KO("저장"),           DE("Speichern"),    FR("Enregistrer"),  ES("Guardar"),
    PT("Salvar"),        IT("Salva"),        NL("Opslaan"),      RU("Сохранить"),
    TR("Kaydet"));
SS_MSG(preset_overwrite_button,
    EN("Replace"),       JA("上書き"),         ZH_HANS("替换"),     ZH_HANT("取代"),
    KO("덮어쓰기"),        DE("Ersetzen"),     FR("Remplacer"),    ES("Reemplazar"),
    PT("Substituir"),    IT("Sostituisci"),  NL("Vervangen"),    RU("Заменить"),
    TR("Değiştir"));
SS_MSG(preset_saved,
    EN("Preset saved: {0}"),
    JA("プリセットを保存しました: {0}"),
    ZH_HANS("预设已保存：{0}"),
    ZH_HANT("預設已儲存：{0}"),
    KO("프리셋을 저장했습니다: {0}"),
    DE("Voreinstellung gespeichert: {0}"),
    FR("Préréglage enregistré : {0}"),
    ES("Ajuste guardado: {0}"),
    PT("Predefinição salva: {0}"),
    IT("Preimpostazione salvata: {0}"),
    NL("Voorinstelling opgeslagen: {0}"),
    RU("Пресет сохранён: {0}"),
    TR("Hazır ayar kaydedildi: {0}"));
SS_MSG(preset_loaded,
    EN("Preset loaded: {0}"),
    JA("プリセットを読み込みました: {0}"),
    ZH_HANS("预设已加载：{0}"),
    ZH_HANT("預設已載入：{0}"),
    KO("프리셋을 불러왔습니다: {0}"),
    DE("Voreinstellung geladen: {0}"),
    FR("Préréglage chargé : {0}"),
    ES("Ajuste cargado: {0}"),
    PT("Predefinição carregada: {0}"),
    IT("Preimpostazione caricata: {0}"),
    NL("Voorinstelling geladen: {0}"),
    RU("Пресет загружен: {0}"),
    TR("Hazır ayar yüklendi: {0}"));
SS_MSG(preset_failed,
    EN("The preset could not be read: {0}"),
    JA("プリセットを読み込めませんでした: {0}"),
    ZH_HANS("无法读取这个预设：{0}"),
    ZH_HANT("無法讀取這個預設：{0}"),
    KO("프리셋을 읽지 못했습니다: {0}"),
    DE("Die Voreinstellung konnte nicht gelesen werden: {0}"),
    FR("Le préréglage n'a pas pu être lu : {0}"),
    ES("No se pudo leer el ajuste: {0}"),
    PT("Não foi possível ler a predefinição: {0}"),
    IT("Non è stato possibile leggere la preimpostazione: {0}"),
    NL("De voorinstelling kon niet gelezen worden: {0}"),
    RU("Не удалось прочитать пресет: {0}"),
    TR("Hazır ayar okunamadı: {0}"));
SS_MSG(preset_pick_file,
    EN("Choose a preset file"),
    JA("プリセットファイルを選ぶ"),
    ZH_HANS("选择一个预设文件"),
    ZH_HANT("選擇一個預設檔"),
    KO("프리셋 파일 선택"),
    DE("Voreinstellungsdatei wählen"),
    FR("Choisir un fichier de préréglage"),
    ES("Elegir un archivo de ajuste"),
    PT("Escolher um arquivo de predefinição"),
    IT("Scegli un file di preimpostazione"),
    NL("Kies een voorinstellingsbestand"),
    RU("Выберите файл пресета"),
    TR("Bir hazır ayar dosyası seçin"));
SS_MSG(preset_pick_folder,
    EN("Choose where to save the preset"),
    JA("プリセットの保存先を選ぶ"),
    ZH_HANS("选择预设的保存位置"),
    ZH_HANT("選擇預設的儲存位置"),
    KO("프리셋을 저장할 위치 선택"),
    DE("Speicherort für die Voreinstellung wählen"),
    FR("Choisir où enregistrer le préréglage"),
    ES("Elegir dónde guardar el ajuste"),
    PT("Escolher onde salvar a predefinição"),
    IT("Scegli dove salvare la preimpostazione"),
    NL("Kies waar de voorinstelling wordt opgeslagen"),
    RU("Выберите, куда сохранить пресет"),
    TR("Hazır ayarın nereye kaydedileceğini seçin"));

SS_MSG(preset_delete,
    EN("Delete this preset..."),
    JA("このプリセットを削除…"),
    ZH_HANS("删除这个预设…"),
    ZH_HANT("刪除這個預設…"),
    KO("이 프리셋 삭제…"),
    DE("Diese Voreinstellung löschen …"),
    FR("Supprimer ce préréglage…"),
    ES("Eliminar este ajuste…"),
    PT("Excluir esta predefinição…"),
    IT("Elimina questa preimpostazione…"),
    NL("Deze voorinstelling verwijderen…"),
    RU("Удалить этот пресет…"),
    TR("Bu hazır ayarı sil…"));
SS_MSG(preset_delete_help,
    EN("Remove the selected preset's file. The options on screen are left as they are -- this deletes the saved copy, not what you are working on."),
    JA("選んでいるプリセットのファイルを消します。画面上の設定はそのままです。消えるのは保存された控えであって、いま編集中の内容ではありません。"),
    ZH_HANS("删除所选预设对应的文件。屏幕上的选项保持不变——删掉的是保存下来的那份副本，不是你正在改的内容。"),
    ZH_HANT("刪除所選預設對應的檔案。畫面上的選項保持不變——刪掉的是儲存下來的那份副本，不是你正在改的內容。"),
    KO("선택한 프리셋의 파일을 지웁니다. 화면의 옵션은 그대로 둡니다. 지워지는 것은 저장해 둔 사본이지, 지금 작업 중인 내용이 아닙니다."),
    DE("Löscht die gewählte Preset-Datei. Die aktuellen Einstellungen auf dem Bildschirm bleiben erhalten."),
    FR("Supprimer le fichier du préréglage sélectionné. Les options à l'écran ne bougent pas : c'est la copie enregistrée qui disparaît, pas ce sur quoi vous travaillez."),
    ES("Eliminar el archivo del ajuste seleccionado. Las opciones en pantalla no cambian: se borra la copia guardada, no aquello en lo que estás trabajando."),
    PT("Remover o arquivo da predefinição selecionada. As opções na tela ficam como estão: some a cópia salva, não aquilo em que você está trabalhando."),
    IT("Elimina il file della preimpostazione selezionata. Le opzioni a schermo restano come sono: sparisce la copia salvata, non ciò su cui stai lavorando."),
    NL("Het bestand van de gekozen voorinstelling verwijderen. De opties op het scherm blijven staan -- weg gaat de opgeslagen kopie, niet waar je mee bezig bent."),
    RU("Удалить файл выбранного пресета. Параметры на экране останутся как есть: пропадает сохранённая копия, а не то, над чем вы работаете."),
    TR("Seçili hazır ayarın dosyasını siler. Ekrandaki seçenekler olduğu gibi kalır -- silinen, kaydedilmiş kopyadır; üzerinde çalıştığınız şey değil."));
SS_MSG(preset_delete_title,
    EN("Delete Preset"),
    JA("プリセットを削除"),
    ZH_HANS("删除预设"),
    ZH_HANT("刪除預設"),
    KO("프리셋 삭제"),
    DE("Voreinstellung löschen"),
    FR("Supprimer le préréglage"),
    ES("Eliminar el ajuste"),
    PT("Excluir a predefinição"),
    IT("Elimina la preimpostazione"),
    NL("Voorinstelling verwijderen"),
    RU("Удаление пресета"),
    TR("Hazır ayarı sil"));
SS_MSG(preset_delete_confirm,
    EN("Delete the preset \"{0}\"?"),
    JA("プリセット「{0}」を削除しますか？"),
    ZH_HANS("要删除预设「{0}」吗？"),
    ZH_HANT("要刪除預設「{0}」嗎？"),
    KO("프리셋 ‘{0}’을(를) 삭제할까요?"),
    DE("Die Voreinstellung „{0}“ löschen?"),
    FR("Supprimer le préréglage « {0} » ?"),
    ES("¿Eliminar el ajuste «{0}»?"),
    PT("Excluir a predefinição “{0}”?"),
    IT("Eliminare la preimpostazione «{0}»?"),
    NL("De voorinstelling „{0}” verwijderen?"),
    RU("Удалить пресет «{0}»?"),
    TR("«{0}» hazır ayarı silinsin mi?"));
SS_MSG(preset_delete_button,
    EN("Delete"),        JA("削除"),          ZH_HANS("删除"),     ZH_HANT("刪除"),
    KO("삭제"),           DE("Löschen"),      FR("Supprimer"),    ES("Eliminar"),
    PT("Excluir"),       IT("Elimina"),      NL("Verwijderen"),  RU("Удалить"),
    TR("Sil"));
SS_MSG(preset_deleted,
    EN("Preset deleted: {0}"),
    JA("プリセットを削除しました: {0}"),
    ZH_HANS("预设已删除：{0}"),
    ZH_HANT("預設已刪除：{0}"),
    KO("프리셋을 삭제했습니다: {0}"),
    DE("Voreinstellung gelöscht: {0}"),
    FR("Préréglage supprimé : {0}"),
    ES("Ajuste eliminado: {0}"),
    PT("Predefinição excluída: {0}"),
    IT("Preimpostazione eliminata: {0}"),
    NL("Voorinstelling verwijderd: {0}"),
    RU("Пресет удалён: {0}"),
    TR("Hazır ayar silindi: {0}"));
SS_MSG(preset_delete_failed,
    EN("The preset could not be deleted: {0}"),
    JA("プリセットを削除できませんでした: {0}"),
    ZH_HANS("无法删除这个预设：{0}"),
    ZH_HANT("無法刪除這個預設：{0}"),
    KO("프리셋을 삭제하지 못했습니다: {0}"),
    DE("Die Voreinstellung konnte nicht gelöscht werden: {0}"),
    FR("Le préréglage n'a pas pu être supprimé : {0}"),
    ES("No se pudo eliminar el ajuste: {0}"),
    PT("Não foi possível excluir a predefinição: {0}"),
    IT("Non è stato possibile eliminare la preimpostazione: {0}"),
    NL("De voorinstelling kon niet verwijderd worden: {0}"),
    RU("Не удалось удалить пресет: {0}"),
    TR("Hazır ayar silinemedi: {0}"));


// ===========================================================================
// Batch processing
// ===========================================================================

SS_MSG(home_batch,
    EN("Batch Processing"),
    JA("バッチ処理"),
    ZH_HANS("批量处理"),
    ZH_HANT("批次處理"),
    KO("일괄 처리"),
    DE("Stapelverarbeitung"),
    FR("Traitement par lots"),
    ES("Procesamiento por lotes"),
    PT("Processamento em lote"),
    IT("Elaborazione in batch"),
    NL("Batchverwerking"),
    RU("Пакетная обработка"),
    TR("Toplu işleme"));
SS_MSG(home_batch_help,
    EN("Queue datasets to build, train and mesh, and work through them one "
       "after another without supervision."),
    JA("データセットの作成・学習・メッシュ化を並べておき、付きっきりでなくても順番に片づけます。"),
    ZH_HANS("把建数据集、训练和生成网格排成队列，不用盯着也能一个接一个做完。"),
    ZH_HANT("把建資料集、訓練與產生網格排成佇列，不用盯著也能一個接一個做完。"),
    KO("데이터셋 생성, 학습, 메시 생성을 줄 세워 두면 지켜보지 않아도 차례로 처리합니다."),
    DE("Datensätze zum Bauen, Trainieren und Meshen in eine Warteschlange "
       "stellen und unbeaufsichtigt nacheinander abarbeiten."),
    FR("Mettre en file la construction de jeux de données, l'entraînement et "
       "le maillage, et les enchaîner sans surveillance."),
    ES("Poner en cola la construcción de conjuntos de datos, el entrenamiento "
       "y el mallado, y encadenarlos sin vigilarlos."),
    PT("Enfileirar a construção de conjuntos de dados, o treinamento e a "
       "geração de malha, e encadeá-los sem supervisão."),
    IT("Mettere in coda la costruzione dei set di dati, l'addestramento e la "
       "mesh, e portarli avanti uno dopo l'altro senza sorvegliarli."),
    NL("Datasets bouwen, trainen en meshen in de wachtrij zetten en zonder "
       "toezicht na elkaar afwerken."),
    RU("Поставить в очередь сборку наборов данных, обучение и построение "
       "полигонов и выполнить всё подряд без присмотра."),
    TR("Veri kümesi kurma, eğitme ve ağ oluşturmayı sıraya koyup, başında "
       "durmadan arka arkaya bitirir."));
SS_MSG(menu_batch,
    EN("Batch Processing..."),
    JA("バッチ処理…"),
    ZH_HANS("批量处理…"),
    ZH_HANT("批次處理…"),
    KO("일괄 처리…"),
    DE("Stapelverarbeitung …"),
    FR("Traitement par lots…"),
    ES("Procesamiento por lotes…"),
    PT("Processamento em lote…"),
    IT("Elaborazione in batch…"),
    NL("Batchverwerking…"),
    RU("Пакетная обработка…"),
    TR("Toplu işleme…"));
SS_MSG(batch_title,
    EN("Batch Processing"),
    JA("バッチ処理"),
    ZH_HANS("批量处理"),
    ZH_HANT("批次處理"),
    KO("일괄 처리"),
    DE("Stapelverarbeitung"),
    FR("Traitement par lots"),
    ES("Procesamiento por lotes"),
    PT("Processamento em lote"),
    IT("Elaborazione in batch"),
    NL("Batchverwerking"),
    RU("Пакетная обработка"),
    TR("Toplu işleme"));
SS_MSG(batch_intro,
    EN("Each row can build a dataset, train it any number of times, and mesh what came out. None of that has to exist yet. A task that fails is recorded and the next one starts anyway."),
    JA("各行はデータセットの作成、その学習 (何回でも)、できたモデルのメッシュ化を行えます。どれもまだ存在していなくて構いません。失敗した作業は記録され、次の作業はそのまま始まります。"),
    ZH_HANS("每一行都可以建数据集、把它训练任意多次，再为训练结果生成网格。这些东西现在都还不必存在。失败的作业会被记录下来，下一个照常开始。"),
    ZH_HANT("每一列都可以建資料集、把它訓練任意多次，再為訓練結果產生網格。這些東西現在都還不必存在。失敗的作業會被記錄下來，下一個照常開始。"),
    KO("각 행은 데이터셋 생성, 반복 학습, 메시 생성을 수행할 수 있습니다. 미리 만들 필요는 없습니다. 실패를 기록하고 다음 작업을 계속합니다."),
    DE("Jede Zeile kann Daten erstellen, mehrfach trainieren und meshen. Nichts muss vorher existieren. Fehler werden vermerkt; die nächste Aufgabe läuft weiter."),
    FR("Chaque ligne peut créer les données, entraîner plusieurs fois et mailler. Rien ne doit préexister. Les échecs sont notés et la tâche suivante démarre."),
    ES("Cada fila puede crear datos, entrenar varias veces y generar mallas. No hace falta crearlos antes. Se registran los fallos y sigue la siguiente tarea."),
    PT("Cada linha pode criar dados, treinar várias vezes e gerar malhas. Nada precisa existir antes. As falhas são registadas e segue a próxima tarefa."),
    IT("Ogni riga può creare dati, addestrare più volte e generare mesh. Non serve prepararli prima. Gli errori sono registrati e si passa all’attività successiva."),
    NL("Elke rij kan data maken, herhaald trainen en meshen. Vooraf aanmaken hoeft niet. Fouten worden genoteerd en de volgende taak gaat door."),
    RU("Строка может создать данные, обучить несколько раз и построить сетку. Готовить их заранее не нужно. Ошибки записываются, очередь продолжается."),
    TR("Her satır veri oluşturabilir, tekrar eğitebilir ve ağ üretebilir. Önceden hazır olmaları gerekmez. Hatalar kaydedilir ve sonraki işe geçilir."));
SS_MSG(batch_drop_hint,
    EN("Drop videos, photo folders, datasets or models here to add rows."),
    JA("動画・写真フォルダー・データセット・モデルをここにドロップすると行が増えます。"),
    ZH_HANS("把视频、照片文件夹、数据集或模型拖到这里就能添加行。"),
    ZH_HANT("把影片、照片資料夾、資料集或模型拖到這裡就能新增列。"),
    KO("동영상, 사진 폴더, 데이터셋, 모델을 여기에 끌어다 놓으면 행이 추가됩니다."),
    DE("Videos, Fotoordner, Datensätze oder Modelle hierher ziehen, um Zeilen "
       "hinzuzufügen."),
    FR("Déposez ici des vidéos, des dossiers de photos, des jeux de données ou "
       "des modèles pour ajouter des lignes."),
    ES("Arrastra aquí vídeos, carpetas de fotos, conjuntos de datos o modelos "
       "para añadir filas."),
    PT("Arraste vídeos, pastas de fotos, conjuntos de dados ou modelos até "
       "aqui para adicionar linhas."),
    IT("Trascina qui video, cartelle di foto, set di dati o modelli per "
       "aggiungere righe."),
    NL("Sleep video's, fotomappen, datasets of modellen hierheen om rijen toe "
       "te voegen."),
    RU("Перетащите сюда видео, папки с фотографиями, наборы данных или модели, "
       "чтобы добавить строки."),
    TR("Satır eklemek için videoları, fotoğraf klasörlerini, veri kümelerini "
       "ya da modelleri buraya bırakın."));
SS_MSG(batch_empty,
    EN("The list is empty. Add a row to get started."),
    JA("一覧が空です。まず行を追加してください。"),
    ZH_HANS("列表是空的。先添加一行吧。"),
    ZH_HANT("清單是空的。先新增一列吧。"),
    KO("목록이 비어 있습니다. 행을 하나 추가해 보세요."),
    DE("Die Liste ist leer. Fügen Sie zum Start eine Zeile hinzu."),
    FR("La liste est vide. Ajoutez une ligne pour commencer."),
    ES("La lista está vacía. Añade una fila para empezar."),
    PT("A lista está vazia. Adicione uma linha para começar."),
    IT("L'elenco è vuoto. Aggiungi una riga per iniziare."),
    NL("De lijst is leeg. Voeg een rij toe om te beginnen."),
    RU("Список пуст. Добавьте строку, чтобы начать."),
    TR("Liste boş. Başlamak için bir satır ekleyin."));
SS_MSG(batch_add_row,
    EN("Add a dataset to train..."),
    JA("学習するデータセットを追加…"),
    ZH_HANS("添加要训练的数据集…"),
    ZH_HANT("新增要訓練的資料集…"),
    KO("학습할 데이터셋 추가…"),
    DE("Datensatz zum Trainieren hinzufügen …"),
    FR("Ajouter un jeu de données à entraîner…"),
    ES("Añadir un conjunto de datos para entrenar…"),
    PT("Adicionar um conjunto de dados para treinar…"),
    IT("Aggiungi un set di dati da addestrare…"),
    NL("Dataset toevoegen om te trainen…"),
    RU("Добавить набор данных для обучения…"),
    TR("Eğitilecek veri kümesi ekle…"));
SS_MSG(batch_add_recent,
    EN("Add a recent one"),
    JA("最近使ったものから追加"),
    ZH_HANS("从最近用过的里添加"),
    ZH_HANT("從最近用過的裡新增"),
    KO("최근 항목에서 추가"),
    DE("Aus den zuletzt benutzten hinzufügen"),
    FR("Ajouter depuis les récents"),
    ES("Añadir uno reciente"),
    PT("Adicionar um dos recentes"),
    IT("Aggiungi da quelli recenti"),
    NL("Een recente toevoegen"),
    RU("Добавить из недавних"),
    TR("Son kullanılanlardan ekle"));
SS_MSG(batch_no_recent,
    EN("No datasets have been opened yet."),
    JA("まだデータセットを開いたことがありません。"),
    ZH_HANS("还没有打开过任何数据集。"),
    ZH_HANT("還沒有開啟過任何資料集。"),
    KO("아직 연 데이터셋이 없습니다."),
    DE("Es wurde noch kein Datensatz geöffnet."),
    FR("Aucun jeu de données n'a encore été ouvert."),
    ES("Todavía no se ha abierto ningún conjunto de datos."),
    PT("Ainda não foi aberto nenhum conjunto de dados."),
    IT("Non è ancora stato aperto nessun set di dati."),
    NL("Er is nog geen dataset geopend."),
    RU("Ни один набор данных ещё не открывался."),
    TR("Henüz hiçbir veri kümesi açılmadı."));
SS_MSG(batch_clear_done,
    EN("Clear done rows"), JA("完了した行を消す"), ZH_HANS("清除已完成的行"),
    ZH_HANT("清除已完成的列"), KO("완료된 행 지우기"), DE("Fertige Zeilen entfernen"),
    FR("Retirer les lignes terminées"), ES("Quitar las filas terminadas"),
    PT("Remover as linhas terminadas"), IT("Rimuovi le righe terminate"),
    NL("Klare rijen verwijderen"), RU("Убрать завершённые строки"),
    TR("Biten satırları temizle"));
SS_MSG(batch_clear_done_help,
    EN("Removes every row whose tasks all finished well the last time it ran. "
       "Rows that failed, were stopped or never ran stay."),
    JA("最後に実行したとき全タスクが成功した行をすべて消します。失敗・停止・未実行の行は"
       "残ります。"),
    ZH_HANS("删除上次运行时所有任务都成功完成的行。失败、被停止或未运行的行保留。"),
    ZH_HANT("刪除上次執行時所有任務都成功完成的列。失敗、被停止或未執行的列保留。"),
    KO("마지막으로 실행했을 때 모든 작업이 잘 끝난 행을 모두 지웁니다. 실패했거나 "
       "중단됐거나 실행되지 않은 행은 남습니다."),
    DE("Entfernt jede Zeile, deren Aufgaben bei ihrem letzten Lauf alle gut "
       "endeten. Fehlgeschlagene, gestoppte oder nie gelaufene Zeilen bleiben."),
    FR("Retire chaque ligne dont toutes les tâches ont bien fini la dernière fois "
       "qu'elle a tourné. Les lignes échouées, arrêtées ou jamais lancées restent."),
    ES("Quita cada fila cuyas tareas terminaron todas bien la última vez que se "
       "ejecutó. Las filas fallidas, detenidas o nunca ejecutadas se quedan."),
    PT("Remove cada linha cujas tarefas terminaram todas bem da última vez que "
       "correu. As linhas falhadas, paradas ou nunca executadas ficam."),
    IT("Rimuove ogni riga i cui compiti sono finiti tutti bene l'ultima volta che "
       "è stata eseguita. Le righe fallite, fermate o mai eseguite restano."),
    NL("Verwijdert elke rij waarvan alle taken de laatste keer dat hij draaide goed "
       "eindigden. Mislukte, gestopte of nooit gedraaide rijen blijven."),
    RU("Убирает каждую строку, все задачи которой успешно завершились при её "
       "последнем запуске. Неудачные, остановленные и не запускавшиеся строки "
       "остаются."),
    TR("Son çalıştığında tüm görevleri iyi biten her satırı kaldırır. Başarısız, "
       "durdurulmuş ya da hiç çalışmamış satırlar kalır."));
SS_MSG(batch_clear_unchecked,
    EN("Clear unchecked rows"), JA("チェックのない行を消す"), ZH_HANS("清除未勾选的行"),
    ZH_HANT("清除未勾選的列"), KO("체크 해제된 행 지우기"),
    DE("Nicht angehakte Zeilen entfernen"), FR("Retirer les lignes non cochées"),
    ES("Quitar las filas sin marcar"), PT("Remover as linhas não marcadas"),
    IT("Rimuovi le righe non spuntate"), NL("Niet-aangevinkte rijen verwijderen"),
    RU("Убрать строки без отметки"), TR("İşaretsiz satırları temizle"));
SS_MSG(batch_clear_unchecked_help,
    EN("Removes every row whose box is unticked, done or not: the ones a run "
       "would leave out."),
    JA("チェックの外れた行を、完了したかどうかに関わらずすべて消します。実行で"
       "飛ばされる行です。"),
    ZH_HANS("删除所有未勾选的行，无论是否已完成：即运行时会跳过的行。"),
    ZH_HANT("刪除所有未勾選的列，無論是否已完成：即執行時會略過的列。"),
    KO("완료 여부와 상관없이 체크가 해제된 행을 모두 지웁니다. 실행 때 건너뛰는 "
       "행들입니다."),
    DE("Entfernt jede Zeile ohne Haken, ob fertig oder nicht: die, die ein Lauf "
       "auslassen würde."),
    FR("Retire chaque ligne décochée, terminée ou non : celles qu'un passage "
       "laisserait de côté."),
    ES("Quita cada fila sin marcar, terminada o no: las que una ejecución dejaría "
       "fuera."),
    PT("Remove cada linha sem marca, terminada ou não: as que uma execução "
       "deixaria de fora."),
    IT("Rimuove ogni riga senza spunta, finita o no: quelle che un'esecuzione "
       "salterebbe."),
    NL("Verwijdert elke rij zonder vinkje, klaar of niet: de rijen die een run "
       "zou overslaan."),
    RU("Убирает каждую строку без отметки, завершённую или нет, — те, что запуск "
       "пропустил бы."),
    TR("İşareti kaldırılmış her satırı, bitmiş olsun olmasın, kaldırır: bir "
       "çalışmanın atlayacağı satırlar."));
SS_MSG(batch_confirm_title,
    EN("Batch list"), JA("バッチ一覧"), ZH_HANS("批处理列表"), ZH_HANT("批次處理列表"),
    KO("배치 목록"), DE("Stapelliste"), FR("Liste du lot"), ES("Lista del lote"),
    PT("Lista do lote"), IT("Lista del lotto"), NL("Batchlijst"), RU("Пакетный список"),
    TR("Toplu liste"));
SS_MSG(batch_clear_confirm,
    EN("Remove every row from the list? The saved presets and the runs already "
       "written are not touched."),
    JA("一覧のすべての行を消しますか？保存済みプリセットと書き出し済みの実行結果は"
       "そのままです。"),
    ZH_HANS("从列表中删除所有行？已保存的预设和已写出的运行结果不受影响。"),
    ZH_HANT("從列表中刪除所有列？已儲存的預設和已寫出的執行結果不受影響。"),
    KO("목록의 모든 행을 지울까요? 저장된 프리셋과 이미 기록된 실행 결과는 그대로입니다."),
    DE("Alle Zeilen von der Liste entfernen? Gespeicherte Presets und bereits "
       "geschriebene Läufe bleiben unberührt."),
    FR("Retirer toutes les lignes de la liste ? Les préréglages enregistrés et les "
       "entraînements déjà écrits ne sont pas touchés."),
    ES("¿Quitar todas las filas de la lista? Los ajustes guardados y las "
       "ejecuciones ya escritas no se tocan."),
    PT("Remover todas as linhas da lista? As predefinições guardadas e os treinos "
       "já escritos não são tocados."),
    IT("Rimuovere tutte le righe dalla lista? I preset salvati e le esecuzioni "
       "già scritte non vengono toccati."),
    NL("Alle rijen van de lijst verwijderen? Opgeslagen presets en al weggeschreven "
       "runs blijven onaangeroerd."),
    RU("Убрать все строки из списка? Сохранённые пресеты и уже записанные запуски "
       "не затрагиваются."),
    TR("Listedeki tüm satırlar kaldırılsın mı? Kayıtlı ön ayarlar ve yazılmış "
       "çalıştırmalar dokunulmadan kalır."));
SS_MSG(batch_clear_done_confirm,
    EN("Remove the rows that finished well? What they wrote stays on disk."),
    JA("成功して終わった行を消しますか？書き出したものはディスクに残ります。"),
    ZH_HANS("删除已成功完成的行？它们写出的内容仍保留在磁盘上。"),
    ZH_HANT("刪除已成功完成的列？它們寫出的內容仍保留在磁碟上。"),
    KO("잘 끝난 행을 지울까요? 그 행들이 기록한 것은 디스크에 남습니다."),
    DE("Die gut beendeten Zeilen entfernen? Was sie geschrieben haben, bleibt auf "
       "der Platte."),
    FR("Retirer les lignes bien terminées ? Ce qu'elles ont écrit reste sur le "
       "disque."),
    ES("¿Quitar las filas que terminaron bien? Lo que escribieron sigue en el disco."),
    PT("Remover as linhas que terminaram bem? O que escreveram fica no disco."),
    IT("Rimuovere le righe finite bene? Ciò che hanno scritto resta su disco."),
    NL("De goed geëindigde rijen verwijderen? Wat ze schreven blijft op schijf."),
    RU("Убрать успешно завершённые строки? Записанное ими остаётся на диске."),
    TR("İyi biten satırlar kaldırılsın mı? Yazdıkları diskte kalır."));
SS_MSG(batch_clear_unchecked_confirm,
    EN("Remove the unticked rows? What they wrote stays on disk."),
    JA("チェックのない行を消しますか？書き出したものはディスクに残ります。"),
    ZH_HANS("删除未勾选的行？它们写出的内容仍保留在磁盘上。"),
    ZH_HANT("刪除未勾選的列？它們寫出的內容仍保留在磁碟上。"),
    KO("체크 해제된 행을 지울까요? 그 행들이 기록한 것은 디스크에 남습니다."),
    DE("Die nicht angehakten Zeilen entfernen? Was sie geschrieben haben, bleibt "
       "auf der Platte."),
    FR("Retirer les lignes non cochées ? Ce qu'elles ont écrit reste sur le "
       "disque."),
    ES("¿Quitar las filas sin marcar? Lo que escribieron sigue en el disco."),
    PT("Remover as linhas não marcadas? O que escreveram fica no disco."),
    IT("Rimuovere le righe non spuntate? Ciò che hanno scritto resta su disco."),
    NL("De niet-aangevinkte rijen verwijderen? Wat ze schreven blijft op schijf."),
    RU("Убрать строки без отметки? Записанное ими остаётся на диске."),
    TR("İşaretsiz satırlar kaldırılsın mı? Yazdıkları diskte kalır."));

SS_MSG(batch_clear,
    EN("Clear list"),
    JA("一覧を空にする"),
    ZH_HANS("清空列表"),
    ZH_HANT("清空清單"),
    KO("목록 비우기"),
    DE("Liste leeren"),
    FR("Vider la liste"),
    ES("Vaciar la lista"),
    PT("Limpar a lista"),
    IT("Svuota l'elenco"),
    NL("Lijst leegmaken"),
    RU("Очистить список"),
    TR("Listeyi temizle"));
SS_MSG(batch_check,
    EN("Check setups"),
    JA("設定を点検"),
    ZH_HANS("检查各行设置"),
    ZH_HANT("檢查各行設定"),
    KO("설정 점검"),
    DE("Einstellungen prüfen"),
    FR("Vérifier les réglages"),
    ES("Comprobar la configuración"),
    PT("Verificar as configurações"),
    IT("Controlla le impostazioni"),
    NL("Instellingen nakijken"),
    RU("Проверить настройки"),
    TR("Ayarları denetle"));
SS_MSG(batch_check_help,
    EN("Checks tasks for missing data, presets or unsupported options without running them."),
    JA("実行せずに、データやプリセットの不足、未対応の設定を確認します。"),
    ZH_HANS("检查任务是否缺少数据、预设或使用了不支持的选项，不会运行任务。"),
    ZH_HANT("檢查任務是否缺少資料、預設或使用了不支援的選項，不會執行任務。"),
    KO("작업을 실행하지 않고 누락된 데이터, 프리셋 또는 미지원 옵션을 확인합니다."),
    DE("Prüft Aufgaben auf fehlende Daten, Presets oder nicht unterstützte Optionen, ohne sie auszuführen."),
    FR("Vérifie les données et préréglages manquants ou les options non prises en charge, sans lancer les tâches."),
    ES("Busca datos o preajustes ausentes y opciones no compatibles sin ejecutar las tareas."),
    PT("Verifica dados ou predefinições em falta e opções não suportadas sem executar tarefas."),
    IT("Controlla dati o preset mancanti e opzioni non supportate senza eseguire le attività."),
    NL("Controleert ontbrekende data of presets en niet-ondersteunde opties zonder taken uit te voeren."),
    RU("Проверяет отсутствие данных и пресетов и неподдерживаемые параметры, не запуская задачи."),
    TR("Görevleri çalıştırmadan eksik veri, önayar veya desteklenmeyen seçenekleri denetler."));
SS_MSG(batch_start,
    EN("Start batch"),
    JA("バッチを開始"),
    ZH_HANS("开始批量训练"),
    ZH_HANT("開始批次訓練"),
    KO("일괄 실행 시작"),
    DE("Stapel starten"),
    FR("Lancer le lot"),
    ES("Iniciar el lote"),
    PT("Iniciar o lote"),
    IT("Avvia il batch"),
    NL("Batch starten"),
    RU("Запустить пакет"),
    TR("Toplu işi başlat"));
SS_MSG(batch_start_skip,
    EN("Skip the bad rows and start"),
    JA("問題のある行を飛ばして開始"),
    ZH_HANS("跳过有问题的行并开始"),
    ZH_HANT("跳過有問題的行並開始"),
    KO("문제가 있는 행은 건너뛰고 시작"),
    DE("Fehlerhafte Zeilen überspringen und starten"),
    FR("Ignorer les lignes en défaut et lancer"),
    ES("Omitir las filas con problemas y empezar"),
    PT("Pular as linhas com problema e começar"),
    IT("Salta le righe con problemi e avvia"),
    NL("Foute rijen overslaan en starten"),
    RU("Пропустить проблемные строки и запустить"),
    TR("Sorunlu satırları atlayıp başlat"));
SS_MSG(batch_blocked,
    EN("Rows with a problem: {0}. Fix them, or start without them."),
    JA("問題のある行: {0}。直すか、その行を除いて始めてください。"),
    ZH_HANS("有问题的行：{0}。请修好它们，或者不带它们开始。"),
    ZH_HANT("有問題的行：{0}。請修好它們，或者不帶它們開始。"),
    KO("문제가 있는 행: {0}. 고치거나, 그 행들을 빼고 시작하세요."),
    DE("Zeilen mit einem Problem: {0}. Beheben Sie sie, oder starten Sie ohne "
       "sie."),
    FR("Lignes avec un problème : {0}. Corrigez-les, ou lancez sans elles."),
    ES("Filas con problemas: {0}. Arréglalas, o empieza sin ellas."),
    PT("Linhas com problema: {0}. Conserte-as, ou comece sem elas."),
    IT("Righe con un problema: {0}. Correggile, oppure parti senza di esse."),
    NL("Rijen met een probleem: {0}. Los ze op, of start zonder ze."),
    RU("Строк с проблемой: {0}. Исправьте их или запустите без них."),
    TR("Sorunlu satır: {0}. Bunları düzeltin ya da onlarsız başlayın."));
SS_MSG(batch_checked_ok,
    EN("Every row looks runnable."),
    JA("どの行も実行できそうです。"),
    ZH_HANS("每一行看起来都能跑。"),
    ZH_HANT("每一行看起來都能跑。"),
    KO("모든 행이 실행 가능해 보입니다."),
    DE("Alle Zeilen sehen lauffähig aus."),
    FR("Toutes les lignes semblent exécutables."),
    ES("Todas las filas parecen ejecutables."),
    PT("Todas as linhas parecem executáveis."),
    IT("Tutte le righe sembrano eseguibili."),
    NL("Elke rij lijkt uitvoerbaar."),
    RU("Все строки выглядят готовыми к запуску."),
    TR("Bütün satırlar çalıştırılabilir görünüyor."));
SS_MSG(batch_no_runnable,
    EN("Nothing to run."),
    JA("実行できるものがありません。"),
    ZH_HANS("没有可运行的内容。"),
    ZH_HANT("沒有可執行的內容。"),
    KO("실행할 것이 없습니다."),
    DE("Es gibt nichts auszuführen."),
    FR("Rien à exécuter."),
    ES("No hay nada que ejecutar."),
    PT("Não há nada para executar."),
    IT("Non c'è nulla da eseguire."),
    NL("Er valt niets uit te voeren."),
    RU("Запускать нечего."),
    TR("Çalıştırılacak bir şey yok."));
// A column heading, so every language is kept to about the width of the
// English one -- the row under it is a number, and a heading that has to be
// truncated says less than a short one.
SS_MSG(batch_col_splats,
    EN("Max splats"),
    JA("スプラット上限"),
    ZH_HANS("泼溅数上限"),
    ZH_HANT("潑濺數上限"),
    KO("스플랫 상한"),
    DE("Max. Splats"),
    FR("Splats max."),
    ES("Splats máx."),
    PT("Splats máx."),
    IT("Splat max."),
    NL("Max. splats"),
    RU("Макс. сплатов"),
    TR("En çok splat"));
SS_MSG(batch_col_sh,
    EN("SH degree"),
    JA("SH 次数"),
    ZH_HANS("SH 阶数"),
    ZH_HANT("SH 階數"),
    KO("SH 차수"),
    DE("SH-Grad"),
    FR("Degré SH"),
    ES("Grado SH"),
    PT("Grau SH"),
    IT("Grado SH"),
    NL("SH-graad"),
    RU("Степень SH"),
    TR("SH derecesi"));
SS_MSG(batch_col_steps,
    EN("Steps"),         JA("ステップ数"),     ZH_HANS("步数"),     ZH_HANT("步數"),
    KO("스텝 수"),         DE("Schritte"),     FR("Étapes"),       ES("Pasos"),
    PT("Passos"),        IT("Passi"),        NL("Stappen"),      RU("Шаги"),
    TR("Adım"));
SS_MSG(batch_col_output,
    EN("Output folder"),
    JA("出力先フォルダー"),
    ZH_HANS("输出文件夹"),
    ZH_HANT("輸出資料夾"),
    KO("출력 폴더"),
    DE("Ausgabeordner"),
    FR("Dossier de sortie"),
    ES("Carpeta de salida"),
    PT("Pasta de saída"),
    IT("Cartella di uscita"),
    NL("Uitvoermap"),
    RU("Папка вывода"),
    TR("Çıktı klasörü"));
SS_MSG(batch_dataset_hint,
    EN("path to a reconstructed dataset"),
    JA("再構成済みデータセットのパス"),
    ZH_HANS("已重建数据集的路径"),
    ZH_HANT("已重建資料集的路徑"),
    KO("재구성이 끝난 데이터셋 경로"),
    DE("Pfad zu einem rekonstruierten Datensatz"),
    FR("chemin d'un jeu de données reconstruit"),
    ES("ruta de un conjunto de datos ya reconstruido"),
    PT("caminho de um conjunto de dados já reconstruído"),
    IT("percorso di un set di dati ricostruito"),
    NL("pad naar een gereconstrueerde dataset"),
    RU("путь к реконструированному набору данных"),
    TR("yeniden oluşturulmuş bir veri kümesinin yolu"));
SS_MSG(floater_per_preset,
    EN("Floater suppression per preset"),
    JA("浮遊ノイズの抑制：プリセットに従う"),
    ZH_HANS("抑制漂浮物：按预设"), ZH_HANT("抑制漂浮物：依預設"),
    KO("프리셋에 따른 부유물 억제"),
    DE("Schwebeteilunterdrückung laut Voreinstellung"),
    FR("Suppression des artefacts flottants selon le préréglage"),
    ES("Supresión de restos flotantes según el ajuste"),
    PT("Supressão de resíduos flutuantes conforme a predefinição"),
    IT("Soppressione dei frammenti fluttuanti da preimpostazione"),
    NL("Zwevende artefacten onderdrukken volgens voorinstelling"),
    RU("Подавление «летающих» артефактов по пресету"),
    TR("Hazır ayara göre uçuşan artıkların bastırılması"));
SS_MSG(floater_off,
    EN("Floater suppression off"),
    JA("浮遊ノイズの抑制：オフ"),
    ZH_HANS("抑制漂浮物：关闭"), ZH_HANT("抑制漂浮物：關閉"),
    KO("부유물 억제 끄기"),
    DE("Schwebeteilunterdrückung aus"),
    FR("Suppression des artefacts flottants désactivée"),
    ES("Supresión de restos flotantes desactivada"),
    PT("Supressão de resíduos flutuantes desativada"),
    IT("Soppressione dei frammenti fluttuanti disattivata"),
    NL("Onderdrukking van zwevende artefacten uit"),
    RU("Подавление «летающих» артефактов выключено"),
    TR("Uçuşan artıkların bastırılması kapalı"));
SS_MSG(floater_mild,
    EN("Mild floater suppression"),
    JA("浮遊ノイズの抑制：弱"),
    ZH_HANS("轻度抑制漂浮物"), ZH_HANT("輕度抑制漂浮物"),
    KO("약한 부유물 억제"),
    DE("Milde Schwebeteilunterdrückung"),
    FR("Suppression légère des artefacts flottants"),
    ES("Supresión leve de restos flotantes"),
    PT("Supressão leve de resíduos flutuantes"),
    IT("Soppressione lieve dei frammenti fluttuanti"),
    NL("Milde onderdrukking van zwevende artefacten"),
    RU("Мягкое подавление «летающих» артефактов"),
    TR("Hafif uçuşan artık bastırma"));
SS_MSG(floater_moderate,
    EN("Moderate floater suppression"),
    JA("浮遊ノイズの抑制：中"),
    ZH_HANS("中度抑制漂浮物"), ZH_HANT("中度抑制漂浮物"),
    KO("보통 부유물 억제"),
    DE("Mittlere Schwebeteilunterdrückung"),
    FR("Suppression modérée des artefacts flottants"),
    ES("Supresión moderada de restos flotantes"),
    PT("Supressão moderada de resíduos flutuantes"),
    IT("Soppressione moderata dei frammenti fluttuanti"),
    NL("Gematigde onderdrukking van zwevende artefacten"),
    RU("Умеренное подавление «летающих» артефактов"),
    TR("Orta düzey uçuşan artık bastırma"));
SS_MSG(floater_strong,
    EN("Strong floater suppression"),
    JA("浮遊ノイズの抑制：強"),
    ZH_HANS("强力抑制漂浮物"), ZH_HANT("強力抑制漂浮物"),
    KO("강한 부유물 억제"),
    DE("Starke Schwebeteilunterdrückung"),
    FR("Suppression forte des artefacts flottants"),
    ES("Supresión fuerte de restos flotantes"),
    PT("Supressão forte de resíduos flutuantes"),
    IT("Soppressione forte dei frammenti fluttuanti"),
    NL("Sterke onderdrukking van zwevende artefacten"),
    RU("Сильное подавление «летающих» артефактов"),
    TR("Güçlü uçuşan artık bastırma"));

SS_MSG(batch_override_hint,
    EN("preset"),        JA("プリセット"),     ZH_HANS("预设"),     ZH_HANT("預設"),
    KO("프리셋"),         DE("Voreinstellung"), FR("préréglage"),  ES("ajuste"),
    PT("predefinição"),  IT("preimpostazione"), NL("voorinstelling"),
    RU("пресет"),        TR("hazır ayar"));
SS_MSG(batch_override_help,
    EN("Overrides what the preset says, for this run alone. Leave it empty to "
       "train with the preset's own value."),
    JA("この実行だけ、プリセットの値を上書きします。空にしておくとプリセットの値のまま学習します。"),
    ZH_HANS("只对这次运行覆盖预设的值。留空就按预设的值训练。"),
    ZH_HANT("只對這次執行覆蓋預設的值。留空就照預設的值訓練。"),
    KO("이 실행에만 프리셋 값을 덮어씁니다. 비워 두면 프리셋 값으로 학습합니다."),
    DE("Überschreibt den Wert der Voreinstellung, nur für diesen Lauf. Leer "
       "lassen, um mit dem Wert der Voreinstellung zu trainieren."),
    FR("Remplace ce que dit le préréglage, pour cette exécution seulement. "
       "Laissez vide pour entraîner avec la valeur du préréglage."),
    ES("Sustituye lo que dice el ajuste, solo para esta ejecución. Déjelo "
       "vacío para entrenar con el valor del ajuste."),
    PT("Substitui o que a predefinição diz, só para esta execução. Deixe vazio "
       "para treinar com o valor da predefinição."),
    IT("Sostituisce ciò che dice la preimpostazione, solo per questa "
       "esecuzione. Lascialo vuoto per addestrare con il valore della "
       "preimpostazione."),
    NL("Overschrijft wat de voorinstelling zegt, alleen voor deze run. Laat "
       "leeg om met de waarde van de voorinstelling te trainen."),
    RU("Переопределяет значение пресета, только для этого прогона. Оставьте "
       "пустым, чтобы обучать со значением пресета."),
    TR("Hazır ayarın değerini yalnızca bu çalıştırma için geçersiz kılar. "
       "Hazır ayarın değeriyle eğitmek için boş bırakın."));
SS_MSG(batch_output_hint,
    EN("default: <dataset>/outputs"),
    JA("既定: <データセット>/outputs"),
    ZH_HANS("默认：<数据集>/outputs"),
    ZH_HANT("預設：<資料集>/outputs"),
    KO("기본값: <데이터셋>/outputs"),
    DE("Standard: <Datensatz>/outputs"),
    FR("par défaut : <jeu de données>/outputs"),
    ES("por defecto: <conjunto de datos>/outputs"),
    PT("padrão: <conjunto de dados>/outputs"),
    IT("predefinito: <set di dati>/outputs"),
    NL("standaard: <dataset>/outputs"),
    RU("по умолчанию: <набор данных>/outputs"),
    TR("varsayılan: <veri kümesi>/outputs"));
SS_MSG(batch_output_help,
    EN("Runs go into this folder, each in its own timestamped subfolder. Leave it empty to write beside the dataset, which is what the trainer screen does."),
    JA("実行結果はこのフォルダーの中に、それぞれ日時のついたサブフォルダーとして入ります。空にすると、学習画面と同じくデータセットの隣に書き出します。"),
    ZH_HANS("每次运行都会放进这个文件夹，各自占一个带时间戳的子文件夹。留空则写在数据集旁边，和训练页面的做法一样。"),
    ZH_HANT("每次執行都會放進這個資料夾，各自佔一個帶時間戳的子資料夾。留空則寫在資料集旁邊，和訓練頁面的做法一樣。"),
    KO("실행 결과는 이 폴더 안에 각각 시각이 붙은 하위 폴더로 들어갑니다. 비워 두면 학습 화면과 마찬가지로 데이터셋 옆에 씁니다."),
    DE("Läufe kommen in diesen Ordner, jeder in einen eigenen Unterordner mit Zeitstempel.\nLeer lassen, um neben dem Datensatz zu schreiben -- so wie es der Trainingsbildschirm tut."),
    FR("Les exécutions vont dans ce dossier, chacune dans un sous-dossier horodaté. Laisser vide pour écrire à côté du jeu de données, comme le fait l'écran d'entraînement."),
    ES("Las ejecuciones van a esta carpeta, cada una en su subcarpeta con fecha y hora.\nDéjalo vacío para escribir junto al conjunto de datos, que es lo que hace la pantalla de entrenamiento."),
    PT("As execuções vão para esta pasta, cada uma em sua subpasta com data e hora. Deixe vazio para escrever ao lado do conjunto de dados, como faz a tela de treinamento."),
    IT("Le esecuzioni finiscono in questa cartella, ciascuna in una sottocartella con data e ora.\nLascialo vuoto per scrivere accanto al set di dati, come fa la schermata di addestramento."),
    NL("Runs komen in deze map, elk in een eigen submap met tijdstempel. Laat het leeg om naast de dataset te schrijven, zoals het trainingsscherm doet."),
    RU("Запуски попадают в эту папку, каждый в свою подпапку с меткой времени. Оставьте пустым, чтобы писать рядом с набором данных, как делает экран обучения."),
    TR("Çalıştırmalar bu klasöre, her biri zaman damgalı kendi alt klasörüne gider. Veri kümesinin yanına yazmak için boş bırakın; eğitim ekranı da böyle yapar."));
SS_MSG(batch_preset_from_file,
    EN("From a file..."),
    JA("ファイルから…"),
    ZH_HANS("从文件…"),
    ZH_HANT("從檔案…"),
    KO("파일에서…"),
    DE("Aus einer Datei …"),
    FR("Depuis un fichier…"),
    ES("Desde un archivo…"),
    PT("De um arquivo…"),
    IT("Da un file…"),
    NL("Uit een bestand…"),
    RU("Из файла…"),
    TR("Bir dosyadan…"));
SS_MSG(batch_pick_dataset,
    EN("Choose a dataset folder"),
    JA("データセットのフォルダーを選ぶ"),
    ZH_HANS("选择数据集文件夹"),
    ZH_HANT("選擇資料集資料夾"),
    KO("데이터셋 폴더 선택"),
    DE("Datensatzordner wählen"),
    FR("Choisir un dossier de jeu de données"),
    ES("Elegir una carpeta de conjunto de datos"),
    PT("Escolher uma pasta de conjunto de dados"),
    IT("Scegli una cartella di set di dati"),
    NL("Kies een datasetmap"),
    RU("Выберите папку набора данных"),
    TR("Bir veri kümesi klasörü seçin"));
SS_MSG(batch_pick_output,
    EN("Choose an output folder"),
    JA("出力先フォルダーを選ぶ"),
    ZH_HANS("选择输出文件夹"),
    ZH_HANT("選擇輸出資料夾"),
    KO("출력 폴더 선택"),
    DE("Ausgabeordner wählen"),
    FR("Choisir un dossier de sortie"),
    ES("Elegir una carpeta de salida"),
    PT("Escolher uma pasta de saída"),
    IT("Scegli una cartella di uscita"),
    NL("Kies een uitvoermap"),
    RU("Выберите папку вывода"),
    TR("Bir çıktı klasörü seçin"));

SS_MSG(batch_status_pending,
    EN("Waiting"),       JA("待機中"),        ZH_HANS("等待中"),   ZH_HANT("等待中"),
    KO("대기 중"),         DE("Wartet"),       FR("En attente"),   ES("En espera"),
    PT("Aguardando"),    IT("In attesa"),    NL("Wacht"),        RU("Ожидает"),
    TR("Bekliyor"));
SS_MSG(batch_status_running,
    EN("Running"),
    JA("実行中"),
    ZH_HANS("运行中"),
    ZH_HANT("執行中"),
    KO("실행 중"),
    DE("Läuft"),
    FR("En cours"),
    ES("En curso"),
    PT("Em execução"),
    IT("In corso"),
    NL("Bezig"),
    RU("Выполняется"),
    TR("Çalışıyor"));
SS_MSG(batch_status_done,
    EN("Done"),          JA("完了"),          ZH_HANS("完成"),     ZH_HANT("完成"),
    KO("완료"),           DE("Fertig"),       FR("Terminé"),      ES("Listo"),
    PT("Concluído"),     IT("Fatto"),        NL("Klaar"),        RU("Готово"),
    TR("Bitti"));
SS_MSG(batch_status_failed,
    EN("Failed"),        JA("失敗"),          ZH_HANS("失败"),     ZH_HANT("失敗"),
    KO("실패"),           DE("Fehlgeschlagen"), FR("Échec"),      ES("Con error"),
    PT("Com falha"),     IT("Non riuscito"), NL("Mislukt"),      RU("Сбой"),
    TR("Başarısız"));
SS_MSG(batch_status_skipped,
    EN("Skipped"),       JA("スキップ"),       ZH_HANS("已跳过"),   ZH_HANT("已跳過"),
    KO("건너뜀"),          DE("Übersprungen"), FR("Ignoré"),       ES("Omitido"),
    PT("Pulado"),        IT("Saltato"),      NL("Overgeslagen"), RU("Пропущено"),
    TR("Atlandı"));
SS_MSG(batch_status_stopped,
    EN("Stopped"),       JA("停止"),          ZH_HANS("已停止"),   ZH_HANT("已停止"),
    KO("중지됨"),          DE("Gestoppt"),     FR("Arrêté"),       ES("Detenido"),
    PT("Interrompido"),  IT("Interrotto"),   NL("Gestopt"),      RU("Остановлено"),
    TR("Durduruldu"));

SS_MSG(batch_issue_row,
    EN("Row {0}: {1}"),
    JA("{0} 行目: {1}"),
    ZH_HANS("第 {0} 行: {1}"),
    ZH_HANT("第 {0} 列: {1}"),
    KO("{0}번 행: {1}"),
    DE("Zeile {0}: {1}"),
    FR("Ligne {0} : {1}"),
    ES("Fila {0}: {1}"),
    PT("Linha {0}: {1}"),
    IT("Riga {0}: {1}"),
    NL("Rij {0}: {1}"),
    RU("Строка {0}: {1}"),
    TR("Satır {0}: {1}"));
SS_MSG(batch_running_banner,
    EN("Batch task {0} of {1}"),
    JA("バッチ作業 {0} / {1}"),
    ZH_HANS("批处理作业 {0} / {1}"),
    ZH_HANT("批次作業 {0} / {1}"),
    KO("일괄 작업 {0} / {1}"),
    DE("Stapelaufgabe {0} von {1}"),
    FR("Tâche de lot {0} sur {1}"),
    ES("Tarea del lote {0} de {1}"),
    PT("Tarefa do lote {0} de {1}"),
    IT("Attività del batch {0} di {1}"),
    NL("Batchtaak {0} van {1}"),
    RU("Задача пакета {0} из {1}"),
    TR("Toplu iş {0} / {1}"));
SS_MSG(confirm_batch,
    EN("Stop training and start the batch?"),
    JA("学習を停止してバッチを開始しますか？"),
    ZH_HANS("要停止训练并开始批量训练吗？"),
    ZH_HANT("要停止訓練並開始批次訓練嗎？"),
    KO("학습을 멈추고 일괄 실행을 시작할까요?"),
    DE("Training anhalten und den Stapel starten?"),
    FR("Arrêter l'entraînement et lancer le lot ?"),
    ES("¿Detener el entrenamiento y empezar el lote?"),
    PT("Parar o treinamento e iniciar o lote?"),
    IT("Fermare l'addestramento e avviare il batch?"),
    NL("Training stoppen en de batch starten?"),
    RU("Остановить обучение и запустить пакет?"),
    TR("Eğitimi durdurup toplu işi başlatalım mı?"));
SS_MSG(batch_show_list,
    EN("Batch list"),
    JA("バッチの一覧"),
    ZH_HANS("批量列表"),
    ZH_HANT("批次清單"),
    KO("일괄 목록"),
    DE("Stapelliste"),
    FR("Liste du lot"),
    ES("Lista del lote"),
    PT("Lista do lote"),
    IT("Elenco del batch"),
    NL("Batchlijst"),
    RU("Список пакета"),
    TR("Toplu iş listesi"));
SS_MSG(batch_show_training,
    EN("Show the run"),
    JA("実行中の画面へ"),
    ZH_HANS("查看正在跑的作业"),
    ZH_HANT("查看正在跑的作業"),
    KO("실행 중인 화면 보기"),
    DE("Den Lauf anzeigen"),
    FR("Afficher l'exécution"),
    ES("Ver la ejecución"),
    PT("Ver a execução"),
    IT("Mostra l'esecuzione"),
    NL("De run tonen"),
    RU("Показать выполнение"),
    TR("Çalışmayı göster"));
SS_MSG(batch_stop_after,
    EN("Stop after this task"),
    JA("この作業のあとで停止"),
    ZH_HANS("做完这个作业后停止"),
    ZH_HANT("做完這個作業後停止"),
    KO("이 작업 후 중지"),
    DE("Nach dieser Aufgabe stoppen"),
    FR("Arrêter après cette tâche"),
    ES("Parar después de esta tarea"),
    PT("Parar depois desta tarefa"),
    IT("Fermati dopo questa attività"),
    NL("Stoppen na deze taak"),
    RU("Остановить после этой задачи"),
    TR("Bu işten sonra dur"));
SS_MSG(batch_stop_after_help,
    EN("Let the task that is running finish, then stop instead of starting the "
       "next one."),
    JA("実行中の作業は最後までやらせて、次を始めずに停止します。"),
    ZH_HANS("让正在跑的作业做完，然后不再开始下一个。"),
    ZH_HANT("讓正在跑的作業做完，然後不再開始下一個。"),
    KO("실행 중인 작업은 끝까지 두고, 다음 작업은 시작하지 않고 멈춥니다."),
    DE("Die laufende Aufgabe zu Ende bringen und dann stoppen, statt die "
       "nächste zu beginnen."),
    FR("Laisser la tâche en cours se terminer, puis s'arrêter au lieu de "
       "démarrer la suivante."),
    ES("Dejar que termine la tarea en curso y luego parar en vez de empezar la "
       "siguiente."),
    PT("Deixar a tarefa em andamento terminar e então parar em vez de começar "
       "a seguinte."),
    IT("Lascia finire l'attività in corso e poi fermati invece di iniziare la "
       "successiva."),
    NL("De lopende taak laten afmaken en dan stoppen in plaats van de volgende "
       "te starten."),
    RU("Дать текущей задаче доработать, а затем остановиться, не начиная "
       "следующую."),
    TR("Çalışan işin bitmesine izin ver, sonra sıradakini başlatmak yerine "
       "dur."));
SS_MSG(batch_stop_now,
    EN("Stop now"),
    JA("いますぐ停止"),
    ZH_HANS("立即停止"),
    ZH_HANT("立即停止"),
    KO("지금 중지"),
    DE("Jetzt anhalten"),
    FR("Arrêter maintenant"),
    ES("Parar ahora"),
    PT("Parar agora"),
    IT("Ferma adesso"),
    NL("Nu stoppen"),
    RU("Остановить сейчас"),
    TR("Şimdi durdur"));
SS_MSG(batch_stop_now_help,
    EN("Cut the running task short -- a training run still saves a checkpoint "
       "-- and stop the batch."),
    JA("実行中の作業を途中で打ち切り、バッチを停止します。学習中ならチェックポイントは保存されます。"),
    ZH_HANS("把正在跑的作业中途切断并停止批处理；如果是训练，仍会保存检查点。"),
    ZH_HANT("把正在跑的作業中途切斷並停止批次處理；如果是訓練，仍會儲存檢查點。"),
    KO("실행 중인 작업을 중간에 끊고 일괄 처리를 멈춥니다. 학습 중이면 체크포인트는 저장됩니다."),
    DE("Die laufende Aufgabe abbrechen -- ein Trainingslauf speichert trotzdem "
       "einen Checkpoint -- und den Stapel stoppen."),
    FR("Interrompre la tâche en cours -- un entraînement enregistre quand même "
       "un point de contrôle -- et arrêter le lot."),
    ES("Cortar la tarea en curso -- un entrenamiento guarda igualmente un "
       "punto de control -- y parar el lote."),
    PT("Cortar a tarefa em andamento -- um treinamento ainda salva um "
       "checkpoint -- e parar o lote."),
    IT("Interrompere l'attività in corso -- un addestramento salva comunque un "
       "checkpoint -- e fermare il batch."),
    NL("De lopende taak afbreken -- een trainingsrun bewaart nog wel een "
       "checkpoint -- en de batch stoppen."),
    RU("Прервать текущую задачу -- обучение всё же сохранит контрольную точку "
       "-- и остановить пакет."),
    TR("Çalışan işi yarıda kes -- bir eğitim yine de kontrol noktası kaydeder "
       "-- ve toplu işi durdur."));
SS_MSG(batch_stopping,
    EN("Stopping after this task."),
    JA("この作業のあとで停止します。"),
    ZH_HANS("将在这个作业之后停止。"),
    ZH_HANT("將在這個作業之後停止。"),
    KO("이 작업 후 중지합니다."),
    DE("Stoppt nach dieser Aufgabe."),
    FR("Arrêt après cette tâche."),
    ES("Se parará después de esta tarea."),
    PT("Vai parar depois desta tarefa."),
    IT("Si fermerà dopo questa attività."),
    NL("Stopt na deze taak."),
    RU("Остановится после этой задачи."),
    TR("Bu işten sonra duracak."));

SS_MSG(batch_log_started,
    EN("Batch started. Tasks: {0}"),
    JA("バッチを開始しました。作業数: {0}"),
    ZH_HANS("批处理已开始。作业数: {0}"),
    ZH_HANT("批次處理已開始。作業數: {0}"),
    KO("일괄 처리를 시작했습니다. 작업 수: {0}"),
    DE("Stapel gestartet. Aufgaben: {0}"),
    FR("Lot démarré. Tâches : {0}"),
    ES("Lote iniciado. Tareas: {0}"),
    PT("Lote iniciado. Tarefas: {0}"),
    IT("Batch avviato. Attività: {0}"),
    NL("Batch gestart. Taken: {0}"),
    RU("Пакет запущен. Задач: {0}"),
    TR("Toplu iş başladı. İş sayısı: {0}"));
SS_MSG(batch_log_job_start,
    EN("Batch job {0}: training {1}"),
    JA("バッチのジョブ {0}: {1} を学習します"),
    ZH_HANS("批量任务 {0}：正在训练 {1}"),
    ZH_HANT("批次工作 {0}：正在訓練 {1}"),
    KO("일괄 작업 {0}: {1} 학습"),
    DE("Stapelauftrag {0}: {1} wird trainiert"),
    FR("Tâche du lot {0} : entraînement de {1}"),
    ES("Trabajo del lote {0}: entrenando {1}"),
    PT("Trabalho do lote {0}: treinando {1}"),
    IT("Lavoro del batch {0}: addestramento di {1}"),
    NL("Batchtaak {0}: {1} wordt getraind"),
    RU("Задание пакета {0}: обучается {1}"),
    TR("Toplu iş {0}: {1} eğitiliyor"));
SS_MSG(batch_log_job_done,
    EN("Batch task {0} finished, written to {1}"),
    JA("バッチ作業 {0} が完了しました。出力先: {1}"),
    ZH_HANS("批处理作业 {0} 已完成，写入 {1}"),
    ZH_HANT("批次作業 {0} 已完成，寫入 {1}"),
    KO("일괄 작업 {0} 이(가) 끝났습니다. 저장 위치: {1}"),
    DE("Stapelaufgabe {0} fertig, geschrieben nach {1}"),
    FR("Tâche de lot {0} terminée, écrite dans {1}"),
    ES("Tarea del lote {0} terminada, escrita en {1}"),
    PT("Tarefa do lote {0} concluída, escrita em {1}"),
    IT("Attività del batch {0} terminata, scritta in {1}"),
    NL("Batchtaak {0} klaar, geschreven naar {1}"),
    RU("Задача пакета {0} завершена, записано в {1}"),
    TR("Toplu iş {0} bitti, {1} konumuna yazıldı"));
SS_MSG(batch_log_job_failed,
    EN("Batch task {0} failed: {1}"),
    JA("バッチ作業 {0} が失敗しました: {1}"),
    ZH_HANS("批处理作业 {0} 失败: {1}"),
    ZH_HANT("批次作業 {0} 失敗: {1}"),
    KO("일괄 작업 {0} 이(가) 실패했습니다: {1}"),
    DE("Stapelaufgabe {0} gescheitert: {1}"),
    FR("Tâche de lot {0} en échec : {1}"),
    ES("La tarea del lote {0} ha fallado: {1}"),
    PT("A tarefa do lote {0} falhou: {1}"),
    IT("L'attività del batch {0} non è riuscita: {1}"),
    NL("Batchtaak {0} mislukt: {1}"),
    RU("Задача пакета {0} не выполнена: {1}"),
    TR("Toplu iş {0} başarısız oldu: {1}"));
SS_MSG(batch_log_job_stopped,
    EN("Batch task {0} was stopped."),
    JA("バッチ作業 {0} を停止しました。"),
    ZH_HANS("批处理作业 {0} 已停止。"),
    ZH_HANT("批次作業 {0} 已停止。"),
    KO("일괄 작업 {0} 을(를) 중지했습니다."),
    DE("Stapelaufgabe {0} wurde gestoppt."),
    FR("Tâche de lot {0} arrêtée."),
    ES("La tarea del lote {0} se ha detenido."),
    PT("A tarefa do lote {0} foi interrompida."),
    IT("L'attività del batch {0} è stata interrotta."),
    NL("Batchtaak {0} is gestopt."),
    RU("Задача пакета {0} остановлена."),
    TR("Toplu iş {0} durduruldu."));
// "Not finished" rather than "not run": a row stopped part-way is in there
// too, and it did run -- it just has no result to report.
SS_MSG(batch_log_summary,
    EN("Batch finished. Done: {0}   Failed: {1}   Not finished: {2}"),
    JA("バッチが終了しました。完了: {0}   失敗: {1}   未完了: {2}"),
    ZH_HANS("批量训练结束。完成：{0}   失败：{1}   未完成：{2}"),
    ZH_HANT("批次訓練結束。完成：{0}   失敗：{1}   未完成：{2}"),
    KO("일괄 실행이 끝났습니다. 완료: {0}   실패: {1}   미완료: {2}"),
    DE("Stapel beendet. Fertig: {0}   Fehlgeschlagen: {1}   Unfertig: {2}"),
    FR("Lot terminé. Terminées : {0}   En échec : {1}   Inachevées : {2}"),
    ES("Lote terminado. Listos: {0}   Con error: {1}   Sin terminar: {2}"),
    PT("Lote concluído. Concluídos: {0}   Com falha: {1}   Não concluídos: {2}"),
    IT("Batch terminato. Fatti: {0}   Non riusciti: {1}   Non finiti: {2}"),
    NL("Batch klaar. Klaar: {0}   Mislukt: {1}   Niet afgemaakt: {2}"),
    RU("Пакет завершён. Готово: {0}   Сбоев: {1}   Не завершено: {2}"),
    TR("Toplu iş bitti. Biten: {0}   Başarısız: {1}   Tamamlanmayan: {2}"));

// ---- the command a finished queue runs ----
// The message these hand over goes through gui::safe_arg(), so an apostrophe
// or a quote in a translation cannot reach the command as syntax.
SS_MSG(batch_cmd_title,
    EN("When the queue finishes"),
    JA("バッチが終わったら"),
    ZH_HANS("批处理结束后"),
    ZH_HANT("批次結束後"),
    KO("일괄 실행이 끝나면"),
    DE("Wenn der Stapel fertig ist"),
    FR("Quand le lot est terminé"),
    ES("Cuando el lote termina"),
    PT("Quando o lote termina"),
    IT("Quando il batch è finito"),
    NL("Als de batch klaar is"),
    RU("Когда пакет завершится"),
    TR("Toplu iş bittiğinde"));
SS_MSG(batch_cmd_help,
    EN("Runs after the last task, even if it failed.\n- {message} becomes a one-line summary, always passed as one argument.\n- Executes without a shell; quotes around the summary are optional."),
    JA("最後のタスクの後に、失敗した場合も実行します。\n- {message} は1行の要約に置き換わり、常に1つの引数になります。\n- シェルを使わず直接実行します。要約を囲む引用符は任意です。"),
    ZH_HANS("最后一个任务结束后运行，即使失败也会执行。\n- {message} 替换为单行摘要，始终作为一个参数传递。\n- 直接执行，不通过 shell；摘要可加或不加引号。"),
    ZH_HANT("最後一個任務結束後執行，即使失敗也會執行。\n- {message} 替換為單行摘要，始終作為一個參數傳遞。\n- 直接執行，不經 shell；摘要可加或不加引號。"),
    KO("마지막 작업 후 실패 여부와 관계없이 실행됩니다.\n- {message}는 한 줄 요약이 되며 항상 단일 인수로 전달됩니다.\n- 셸 없이 직접 실행합니다. 요약을 감싸는 따옴표는 선택 사항입니다."),
    DE("Läuft nach der letzten Aufgabe, auch bei Fehlern.\n- {message} wird zur einzeiligen Zusammenfassung, immer als ein Argument.\n- Direkte Ausführung ohne Shell; Anführungszeichen um die Zusammenfassung sind optional."),
    FR("S’exécute après la dernière tâche, même en cas d’échec.\n- {message} devient un résumé sur une ligne, toujours passé en un seul argument.\n- Exécution sans shell ; les guillemets autour du résumé sont facultatifs."),
    ES("Se ejecuta tras la última tarea, incluso si falla.\n- {message} se sustituye por un resumen de una línea, siempre como un argumento.\n- Ejecución sin shell; las comillas alrededor del resumen son opcionales."),
    PT("Executa após a última tarefa, mesmo que falhe.\n- {message} torna-se um resumo de uma linha, sempre passado como um argumento.\n- Execução sem shell; aspas à volta do resumo são opcionais."),
    IT("Esegue dopo l’ultima attività, anche se fallita.\n- {message} diventa un riepilogo su una riga, sempre come singolo argomento.\n- Esecuzione senza shell; virgolette attorno al riepilogo facoltative."),
    NL("Wordt na de laatste taak uitgevoerd, ook bij fouten.\n- {message} wordt een samenvatting op één regel, altijd als één argument.\n- Uitvoering zonder shell; aanhalingstekens rond de samenvatting zijn optioneel."),
    RU("Выполняется после последней задачи, даже при ошибке.\n- {message} заменяется однострочным итогом, всегда как один аргумент.\n- Запуск без оболочки; кавычки вокруг итога необязательны."),
    TR("Son görevden sonra, başarısız olsa bile çalışır.\n- {message} tek satırlık özete dönüşür ve daima tek argüman olarak iletilir.\n- Kabuk olmadan çalışır; özeti tırnak içine almak isteğe bağlıdır."));
SS_MSG(batch_cmd_test,
    EN("Test"),
    JA("テスト"),
    ZH_HANS("测试"),
    ZH_HANT("測試"),
    KO("테스트"),
    DE("Testen"),
    FR("Tester"),
    ES("Probar"),
    PT("Testar"),
    IT("Prova"),
    NL("Testen"),
    RU("Проверить"),
    TR("Dene"));
SS_MSG(batch_cmd_test_help,
    EN("Runs the command now, with a test message in place of the summary."),
    JA("結果のまとめの代わりにテスト用の文を入れて、コマンドをいま実行します。"),
    ZH_HANS("现在就运行这条命令，用一条测试消息代替结果摘要。"),
    ZH_HANT("現在就執行這條命令，用一則測試訊息代替結果摘要。"),
    KO("요약 대신 테스트 문구를 넣어 명령을 지금 실행합니다."),
    DE("Führt den Befehl jetzt aus, mit einer Testnachricht statt der "
       "Zusammenfassung."),
    FR("Lance la commande maintenant, avec un message de test à la place du "
       "résumé."),
    ES("Ejecuta el comando ahora, con un mensaje de prueba en lugar del "
       "resumen."),
    PT("Executa o comando agora, com uma mensagem de teste no lugar do "
       "resumo."),
    IT("Esegue il comando adesso, con un messaggio di prova al posto del "
       "riassunto."),
    NL("Voert de opdracht nu uit, met een testbericht in plaats van de "
       "samenvatting."),
    RU("Запускает команду сейчас, подставив пробное сообщение вместо сводки."),
    TR("Komutu şimdi çalıştırır, özet yerine bir deneme iletisi koyarak."));
// The product name is the localized one where there is one (brand::product):
// this is in-text copy, not the wordmark, and it is written out per language
// rather than assembled, exactly as brand::window_title is.
SS_MSG(batch_cmd_test_message,
    EN("Spirula Studio: this is a test message."),
    JA("スピルラ・スタジオ: これはテスト用のメッセージです。"),
    ZH_HANS("旋影工坊：这是一条测试消息。"),
    ZH_HANT("旋影工坊：這是一則測試訊息。"),
    KO("스피룰라 스튜디오: 테스트 메시지입니다."),
    DE("Spirula Studio: Das ist eine Testnachricht."),
    FR("Spirula Studio : ceci est un message de test."),
    ES("Spirula Studio: este es un mensaje de prueba."),
    PT("Spirula Studio: esta é uma mensagem de teste."),
    IT("Spirula Studio: questo è un messaggio di prova."),
    NL("Spirula Studio: dit is een testbericht."),
    RU("Spirula Studio: это пробное сообщение."),
    TR("Spirula Studio: bu bir deneme iletisidir."));
SS_MSG(batch_cmd_message,
    EN("Spirula Studio: batch processing finished. Done: {0}, failed: {1}, "
       "not finished: {2}."),
    JA("スピルラ・スタジオ: バッチ処理が終了しました。完了: {0}、失敗: {1}、"
       "未完了: {2}。"),
    ZH_HANS("旋影工坊：批处理结束。完成：{0}，失败：{1}，未完成：{2}。"),
    ZH_HANT("旋影工坊：批次處理結束。完成：{0}，失敗：{1}，未完成：{2}。"),
    KO("스피룰라 스튜디오: 일괄 처리가 끝났습니다. 완료: {0}, 실패: {1}, "
       "미완료: {2}."),
    DE("Spirula Studio: Die Stapelverarbeitung ist fertig. Fertig: {0}, "
       "fehlgeschlagen: {1}, unfertig: {2}."),
    FR("Spirula Studio : le traitement par lots est terminé. Terminées : "
       "{0}, en échec : {1}, inachevées : {2}."),
    ES("Spirula Studio: el procesamiento por lotes ha terminado. Listos: "
       "{0}, con error: {1}, sin terminar: {2}."),
    PT("Spirula Studio: o processamento em lote terminou. Concluídos: {0}, "
       "com falha: {1}, não concluídos: {2}."),
    IT("Spirula Studio: l’elaborazione batch è finita. Fatti: {0}, non "
       "riusciti: {1}, non finiti: {2}."),
    NL("Spirula Studio: de batchverwerking is klaar. Klaar: {0}, mislukt: "
       "{1}, niet afgemaakt: {2}."),
    RU("Spirula Studio: пакетная обработка завершена. Готово: {0}, сбоев: "
       "{1}, не завершено: {2}."),
    TR("Spirula Studio: toplu işlem bitti. Biten: {0}, başarısız: {1}, "
       "tamamlanmayan: {2}."));

// ---- what a pre-flight can find ----
SS_MSG(chk_dataset_empty,
    EN("No dataset folder is set."),
    JA("データセットのフォルダーが設定されていません。"),
    ZH_HANS("没有设置数据集文件夹。"),
    ZH_HANT("沒有設定資料集資料夾。"),
    KO("데이터셋 폴더가 지정되지 않았습니다."),
    DE("Es ist kein Datensatzordner angegeben."),
    FR("Aucun dossier de jeu de données n'est indiqué."),
    ES("No se ha indicado ninguna carpeta de conjunto de datos."),
    PT("Nenhuma pasta de conjunto de dados foi indicada."),
    IT("Non è indicata nessuna cartella di set di dati."),
    NL("Er is geen datasetmap ingesteld."),
    RU("Папка набора данных не задана."),
    TR("Veri kümesi klasörü belirtilmemiş."));
SS_MSG(chk_dataset_missing,
    EN("The dataset folder does not exist: {0}"),
    JA("データセットのフォルダーがありません: {0}"),
    ZH_HANS("数据集文件夹不存在：{0}"),
    ZH_HANT("資料集資料夾不存在：{0}"),
    KO("데이터셋 폴더가 없습니다: {0}"),
    DE("Den Datensatzordner gibt es nicht: {0}"),
    FR("Le dossier du jeu de données n'existe pas : {0}"),
    ES("La carpeta del conjunto de datos no existe: {0}"),
    PT("A pasta do conjunto de dados não existe: {0}"),
    IT("La cartella del set di dati non esiste: {0}"),
    NL("De datasetmap bestaat niet: {0}"),
    RU("Папки набора данных нет: {0}"),
    TR("Veri kümesi klasörü yok: {0}"));
SS_MSG(chk_dataset_not_a_dir,
    EN("This is a file, not a dataset folder: {0}"),
    JA("これはファイルであって、データセットのフォルダーではありません: {0}"),
    ZH_HANS("这是一个文件，不是数据集文件夹：{0}"),
    ZH_HANT("這是一個檔案，不是資料集資料夾：{0}"),
    KO("이것은 파일이지 데이터셋 폴더가 아닙니다: {0}"),
    DE("Das ist eine Datei, kein Datensatzordner: {0}"),
    FR("Ceci est un fichier, pas un dossier de jeu de données : {0}"),
    ES("Esto es un archivo, no una carpeta de conjunto de datos: {0}"),
    PT("Isto é um arquivo, não uma pasta de conjunto de dados: {0}"),
    IT("Questo è un file, non una cartella di set di dati: {0}"),
    NL("Dit is een bestand, geen datasetmap: {0}"),
    RU("Это файл, а не папка набора данных: {0}"),
    TR("Bu bir dosya, veri kümesi klasörü değil: {0}"));
SS_MSG(chk_partition_missing,
    EN("The partition file this row trains a part of is missing: {0}"),
    JA("この行が学習するパートの分割ファイルがありません: {0}"),
    ZH_HANS("此行要训练的分区所属的分区文件不存在：{0}"),
    ZH_HANT("此行要訓練的分區所屬的分區檔案不存在：{0}"),
    KO("이 행이 학습할 파트의 분할 파일이 없습니다: {0}"),
    DE("Die Partitionsdatei, deren Teil diese Zeile trainiert, fehlt: {0}"),
    FR("Le fichier de partition dont cette ligne entraîne une partie est absent : {0}"),
    ES("Falta el archivo de partición del que esta fila entrena una parte: {0}"),
    PT("Falta o ficheiro de partição de que esta linha treina uma parte: {0}"),
    IT("Manca il file di partizione di cui questa riga addestra una parte: {0}"),
    NL("Het partitiebestand waarvan deze rij een deel traint ontbreekt: {0}"),
    RU("Нет файла разбиения, часть которого обучает эта строка: {0}"),
    TR("Bu satırın bir parçasını eğittiği bölümleme dosyası yok: {0}"));

SS_MSG(chk_dataset_unreadable,
    EN("This folder holds no reconstruction the trainer can read -- no "
       "transforms.json, no sparse/ or colmap/, no Metashape .xml beside a "
       ".ply: {0}"),
    JA("このフォルダーには学習側が読める再構成がありません。transforms.json も、"
       "sparse/ や colmap/ も、.ply と並んだ Metashape の .xml もありません: {0}"),
    ZH_HANS("这个文件夹里没有训练端能读的重建结果——没有 transforms.json，没有 "
            "sparse/ 或 colmap/，也没有和 .ply 放在一起的 Metashape .xml：{0}"),
    ZH_HANT("這個資料夾裡沒有訓練端能讀的重建結果——沒有 transforms.json，沒有 "
            "sparse/ 或 colmap/，也沒有和 .ply 放在一起的 Metashape .xml：{0}"),
    KO("이 폴더에는 학습기가 읽을 수 있는 재구성이 없습니다. transforms.json도, "
       "sparse/나 colmap/도, .ply 옆의 Metashape .xml도 없습니다: {0}"),
    DE("In diesem Ordner liegt keine Rekonstruktion, die das Training lesen "
       "kann -- keine transforms.json, kein sparse/ oder colmap/, keine "
       "Metashape-.xml neben einer .ply: {0}"),
    FR("Ce dossier ne contient aucune reconstruction lisible par "
       "l'entraînement : ni transforms.json, ni sparse/ ou colmap/, ni .xml "
       "Metashape à côté d'un .ply : {0}"),
    ES("Esta carpeta no contiene ninguna reconstrucción que el entrenamiento "
       "pueda leer: ni transforms.json, ni sparse/ o colmap/, ni un .xml de "
       "Metashape junto a un .ply: {0}"),
    PT("Esta pasta não contém nenhuma reconstrução que o treinamento consiga "
       "ler: nem transforms.json, nem sparse/ ou colmap/, nem um .xml do "
       "Metashape ao lado de um .ply: {0}"),
    IT("Questa cartella non contiene nessuna ricostruzione leggibile "
       "dall'addestramento: né transforms.json, né sparse/ o colmap/, né un "
       ".xml di Metashape accanto a un .ply: {0}"),
    NL("In deze map staat geen reconstructie die de training kan lezen -- geen "
       "transforms.json, geen sparse/ of colmap/, geen Metashape-.xml naast "
       "een .ply: {0}"),
    RU("В этой папке нет реконструкции, которую обучение может прочитать: ни "
       "transforms.json, ни sparse/ или colmap/, ни .xml от Metashape рядом с "
       ".ply: {0}"),
    TR("Bu klasörde eğitimin okuyabileceği bir yeniden oluşturma yok -- ne "
       "transforms.json, ne sparse/ ya da colmap/, ne de bir .ply yanında "
       "Metashape .xml dosyası: {0}"));
// Two rows on one dataset are ordinary -- comparing presets, or sweeping the
// splat budget, is what a batch is for. Only rows that would do exactly the
// same work are worth saying anything about.
SS_MSG(chk_dataset_duplicate,
    EN("Another row trains this dataset with the same settings: {0}"),
    JA("同じ設定でこのデータセットを学習する行が他にもあります: {0}"),
    ZH_HANS("还有一行用同样的设置训练这个数据集：{0}"),
    ZH_HANT("還有一行用同樣的設定訓練這個資料集：{0}"),
    KO("같은 설정으로 이 데이터셋을 학습하는 행이 또 있습니다: {0}"),
    DE("Eine andere Zeile trainiert diesen Datensatz mit denselben "
       "Einstellungen: {0}"),
    FR("Une autre ligne entraîne ce jeu de données avec les mêmes réglages : "
       "{0}"),
    ES("Otra fila entrena este conjunto de datos con los mismos ajustes: {0}"),
    PT("Outra linha treina este conjunto de dados com as mesmas configurações: "
       "{0}"),
    IT("Un'altra riga addestra questo set di dati con le stesse impostazioni: "
       "{0}"),
    NL("Een andere rij traint deze dataset met dezelfde instellingen: {0}"),
    RU("Другая строка обучает этот набор данных с теми же настройками: {0}"),
    TR("Başka bir satır bu veri kümesini aynı ayarlarla eğitiyor: {0}"));
SS_MSG(chk_bad_max_splats,
    EN("Max splats must be a whole number of 1 or more: {0}"),
    JA("スプラット数の上限は 1 以上の整数にしてください: {0}"),
    ZH_HANS("泼溅数上限必须是 1 或更大的整数：{0}"),
    ZH_HANT("潑濺數上限必須是 1 或更大的整數：{0}"),
    KO("최대 스플랫 수는 1 이상의 정수여야 합니다: {0}"),
    DE("Die Höchstzahl der Splats muss eine ganze Zahl ab 1 sein: {0}"),
    FR("Le nombre maximal de splats doit être un entier supérieur ou égal à "
       "1 : {0}"),
    ES("El número máximo de splats debe ser un entero de 1 o más: {0}"),
    PT("O número máximo de splats precisa ser um inteiro de 1 ou mais: {0}"),
    IT("Il numero massimo di splat deve essere un intero maggiore o uguale a "
       "1: {0}"),
    NL("Het maximum aantal splats moet een geheel getal van 1 of meer zijn: "
       "{0}"),
    RU("Максимум сплатов должен быть целым числом от 1: {0}"),
    TR("En fazla splat sayısı 1 veya daha büyük bir tam sayı olmalı: {0}"));
SS_MSG(chk_bad_sh_degree,
    EN("SH degree must be a whole number from 0 to 4: {0}"),
    JA("SH 次数は 0 から 4 までの整数にしてください: {0}"),
    ZH_HANS("SH 阶数必须是 0 到 4 之间的整数：{0}"),
    ZH_HANT("SH 階數必須是 0 到 4 之間的整數：{0}"),
    KO("SH 차수는 0에서 4 사이의 정수여야 합니다: {0}"),
    DE("Der SH-Grad muss eine ganze Zahl von 0 bis 4 sein: {0}"),
    FR("Le degré SH doit être un entier de 0 à 4 : {0}"),
    ES("El grado SH debe ser un entero de 0 a 4: {0}"),
    PT("O grau SH precisa ser um inteiro de 0 a 4: {0}"),
    IT("Il grado SH deve essere un intero da 0 a 4: {0}"),
    NL("De SH-graad moet een geheel getal van 0 tot en met 4 zijn: {0}"),
    RU("Степень SH должна быть целым числом от 0 до 4: {0}"),
    TR("SH derecesi 0 ile 4 arasında bir tam sayı olmalı: {0}"));
SS_MSG(chk_bad_steps,
    EN("Steps must be a whole number of 1 or more: {0}"),
    JA("ステップ数は 1 以上の整数にしてください: {0}"),
    ZH_HANS("步数必须是 1 或更大的整数：{0}"),
    ZH_HANT("步數必須是 1 或更大的整數：{0}"),
    KO("스텝 수는 1 이상의 정수여야 합니다: {0}"),
    DE("Die Schrittzahl muss eine ganze Zahl ab 1 sein: {0}"),
    FR("Le nombre d'étapes doit être un entier supérieur ou égal à 1 : {0}"),
    ES("Los pasos deben ser un entero de 1 o más: {0}"),
    PT("Os passos precisam ser um inteiro de 1 ou mais: {0}"),
    IT("I passi devono essere un intero maggiore o uguale a 1: {0}"),
    NL("Het aantal stappen moet een geheel getal van 1 of meer zijn: {0}"),
    RU("Число шагов должно быть целым числом от 1: {0}"),
    TR("Adım sayısı 1 veya daha büyük bir tam sayı olmalı: {0}"));
SS_MSG(chk_images_missing,
    EN("There is no '{0}' folder in the dataset. The parser may still find the "
       "photos where the reconstruction says they are."),
    JA("データセットに「{0}」フォルダーがありません。再構成が示す場所に写真が"
       "あれば、読み込み側がそちらを見つけられることもあります。"),
    ZH_HANS("数据集里没有「{0}」文件夹。如果照片就在重建结果指出的位置，解析器"
            "仍有可能找到它们。"),
    ZH_HANT("資料集裡沒有「{0}」資料夾。如果照片就在重建結果指出的位置，解析器"
            "仍有可能找到它們。"),
    KO("데이터셋에 ‘{0}’ 폴더가 없습니다. 재구성이 가리키는 자리에 사진이 있다면 "
       "파서가 그쪽에서 찾아낼 수도 있습니다."),
    DE("Im Datensatz gibt es keinen Ordner „{0}“. Der Parser findet die Fotos "
       "womöglich trotzdem dort, wo die Rekonstruktion sie verortet."),
    FR("Il n'y a pas de dossier « {0} » dans le jeu de données. L'analyseur "
       "peut tout de même trouver les photos là où la reconstruction les situe."),
    ES("No hay ninguna carpeta «{0}» en el conjunto de datos. El analizador "
       "todavía puede encontrar las fotos donde la reconstrucción dice que "
       "están."),
    PT("Não há pasta “{0}” no conjunto de dados. O analisador ainda pode "
       "encontrar as fotos onde a reconstrução diz que elas estão."),
    IT("Nel set di dati non c'è nessuna cartella «{0}». Il parser potrebbe "
       "comunque trovare le foto dove le colloca la ricostruzione."),
    NL("Er is geen map „{0}” in de dataset. De parser vindt de foto's misschien "
       "toch, daar waar de reconstructie zegt dat ze staan."),
    RU("В наборе данных нет папки «{0}». Разбор всё же может найти снимки там, "
       "где их указывает реконструкция."),
    TR("Veri kümesinde «{0}» klasörü yok. Ayrıştırıcı fotoğrafları yine de "
       "yeniden oluşturmanın gösterdiği yerde bulabilir."));
SS_MSG(chk_preset_missing,
    EN("The preset file is gone: {0}"),
    JA("プリセットのファイルがなくなっています: {0}"),
    ZH_HANS("预设文件已经不在了：{0}"),
    ZH_HANT("預設檔已經不在了：{0}"),
    KO("프리셋 파일이 사라졌습니다: {0}"),
    DE("Die Voreinstellungsdatei ist verschwunden: {0}"),
    FR("Le fichier de préréglage a disparu : {0}"),
    ES("El archivo de ajuste ha desaparecido: {0}"),
    PT("O arquivo de predefinição sumiu: {0}"),
    IT("Il file di preimpostazione è sparito: {0}"),
    NL("Het voorinstellingsbestand is weg: {0}"),
    RU("Файл пресета пропал: {0}"),
    TR("Hazır ayar dosyası kaybolmuş: {0}"));
SS_MSG(chk_preset_unreadable,
    EN("The preset file could not be read: {0}"),
    JA("プリセットのファイルを読み込めませんでした: {0}"),
    ZH_HANS("无法读取预设文件：{0}"),
    ZH_HANT("無法讀取預設檔：{0}"),
    KO("프리셋 파일을 읽지 못했습니다: {0}"),
    DE("Die Voreinstellungsdatei konnte nicht gelesen werden: {0}"),
    FR("Le fichier de préréglage n'a pas pu être lu : {0}"),
    ES("No se pudo leer el archivo de ajuste: {0}"),
    PT("Não foi possível ler o arquivo de predefinição: {0}"),
    IT("Non è stato possibile leggere il file di preimpostazione: {0}"),
    NL("Het voorinstellingsbestand kon niet gelezen worden: {0}"),
    RU("Не удалось прочитать файл пресета: {0}"),
    TR("Hazır ayar dosyası okunamadı: {0}"));
SS_MSG(chk_preset_unknown,
    EN("There is no built-in preset by this name: {0}"),
    JA("この名前の組み込みプリセットはありません: {0}"),
    ZH_HANS("没有叫这个名字的内置预设：{0}"),
    ZH_HANT("沒有叫這個名字的內建預設：{0}"),
    KO("이 이름의 기본 제공 프리셋은 없습니다: {0}"),
    DE("Es gibt keine mitgelieferte Voreinstellung dieses Namens: {0}"),
    FR("Il n'existe aucun préréglage fourni portant ce nom : {0}"),
    ES("No hay ningún ajuste incluido con ese nombre: {0}"),
    PT("Não existe predefinição incluída com esse nome: {0}"),
    IT("Non esiste nessuna preimpostazione inclusa con questo nome: {0}"),
    NL("Er is geen ingebouwde voorinstelling met deze naam: {0}"),
    RU("Встроенного пресета с таким названием нет: {0}"),
    TR("Bu adda yerleşik bir hazır ayar yok: {0}"));
SS_MSG(chk_output_unusable,
    EN("The output folder cannot be created -- no part of this path exists: {0}"),
    JA("出力先フォルダーを作れません。このパスはどの部分も存在しません: {0}"),
    ZH_HANS("无法创建输出文件夹——这条路径没有任何一段是存在的：{0}"),
    ZH_HANT("無法建立輸出資料夾——這條路徑沒有任何一段是存在的：{0}"),
    KO("출력 폴더를 만들 수 없습니다. 이 경로는 어느 부분도 존재하지 않습니다: {0}"),
    DE("Der Ausgabeordner lässt sich nicht anlegen -- kein Teil dieses Pfads "
       "existiert: {0}"),
    FR("Le dossier de sortie ne peut pas être créé : aucune partie de ce chemin "
       "n'existe : {0}"),
    ES("No se puede crear la carpeta de salida: ninguna parte de esta ruta "
       "existe: {0}"),
    PT("Não dá para criar a pasta de saída: nenhuma parte deste caminho existe: "
       "{0}"),
    IT("La cartella di uscita non si può creare: nessuna parte di questo "
       "percorso esiste: {0}"),
    NL("De uitvoermap kan niet worden aangemaakt -- geen enkel deel van dit pad "
       "bestaat: {0}"),
    RU("Папку вывода не создать: ни одной части этого пути не существует: {0}"),
    TR("Çıktı klasörü oluşturulamıyor -- bu yolun hiçbir parçası yok: {0}"));
SS_MSG(chk_output_is_file,
    EN("The output path is a file: {0}"),
    JA("出力先のパスがファイルになっています: {0}"),
    ZH_HANS("输出路径指向的是一个文件：{0}"),
    ZH_HANT("輸出路徑指向的是一個檔案：{0}"),
    KO("출력 경로가 파일입니다: {0}"),
    DE("Der Ausgabepfad ist eine Datei: {0}"),
    FR("Le chemin de sortie est un fichier : {0}"),
    ES("La ruta de salida es un archivo: {0}"),
    PT("O caminho de saída é um arquivo: {0}"),
    IT("Il percorso di uscita è un file: {0}"),
    NL("Het uitvoerpad is een bestand: {0}"),
    RU("Путь вывода указывает на файл: {0}"),
    TR("Çıktı yolu bir dosya: {0}"));
SS_MSG(chk_unsupported,
    EN("This preset asks for something the trainer does not do: {0}"),
    JA("このプリセットは学習側が対応していない指定を含んでいます: {0}"),
    ZH_HANS("这个预设里有训练端做不到的要求：{0}"),
    ZH_HANT("這個預設裡有訓練端做不到的要求：{0}"),
    KO("이 프리셋에는 학습기가 하지 못하는 요구가 들어 있습니다: {0}"),
    DE("Diese Voreinstellung verlangt etwas, das das Training nicht kann: {0}"),
    FR("Ce préréglage demande quelque chose que l'entraînement ne sait pas "
       "faire : {0}"),
    ES("Este ajuste pide algo que el entrenamiento no hace: {0}"),
    PT("Esta predefinição pede algo que o treinamento não faz: {0}"),
    IT("Questa preimpostazione chiede qualcosa che l'addestramento non fa: {0}"),
    NL("Deze voorinstelling vraagt iets wat de training niet doet: {0}"),
    RU("Этот пресет требует того, чего обучение не умеет: {0}"),
    TR("Bu hazır ayar, eğitimin yapmadığı bir şey istiyor: {0}"));
SS_MSG(chk_no_device,
    EN("No usable GPU was found; nothing can train."),
    JA("使える GPU が見つかりません。学習は行えません。"),
    ZH_HANS("没有找到可用的 GPU，什么都训练不了。"),
    ZH_HANT("沒有找到可用的 GPU，什麼都訓練不了。"),
    KO("쓸 수 있는 GPU를 찾지 못했습니다. 아무것도 학습할 수 없습니다."),
    DE("Es wurde keine nutzbare GPU gefunden; es kann nichts trainiert werden."),
    FR("Aucun GPU utilisable n'a été trouvé ; rien ne peut être entraîné."),
    ES("No se encontró ninguna GPU utilizable; no se puede entrenar nada."),
    PT("Nenhuma GPU utilizável foi encontrada; nada pode ser treinado."),
    IT("Non è stata trovata nessuna GPU utilizzabile; non si può addestrare "
       "nulla."),
    NL("Er is geen bruikbare GPU gevonden; er kan niets getraind worden."),
    RU("Пригодный GPU не найден; обучать нечем."),
    TR("Kullanılabilir GPU bulunamadı; hiçbir şey eğitilemez."));


// ---------------------------------------------------------------------------
// Mesh: the "create mesh from splats" screen, and viewing a mesh file
// ---------------------------------------------------------------------------

SS_MSG(home_make_mesh,
    EN("Create a mesh from splats"),
    JA("スプラットからメッシュを作る"),
    ZH_HANS("从高斯点生成网格"),
    ZH_HANT("從高斯點產生網格"),
    KO("스플랫에서 메시 만들기"),
    DE("Netz aus Splats erzeugen"),
    FR("Créer un maillage à partir des splats"),
    ES("Crear una malla a partir de los splats"),
    PT("Criar uma malha a partir dos splats"),
    IT("Crea una mesh dagli splat"),
    NL("Mesh maken van splats"),
    RU("Построить меш из сплатов"),
    TR("Splat'lardan ağ oluştur"));

SS_MSG(home_make_mesh_help,
    EN("Create a mesh from a trained model for Blender, printing or games. Available training photos improve the surface."),
    JA("学習済みモデルからBlender、印刷、ゲーム用のメッシュを作成します。学習写真が残っていれば表面が改善します。"),
    ZH_HANS("将训练好的模型生成用于 Blender、打印或游戏的网格。保留的训练照片可改善表面。"),
    ZH_HANT("將訓練好的模型產生用於 Blender、列印或遊戲的網格。保留的訓練照片可改善表面。"),
    KO("학습 모델로 Blender, 인쇄 또는 게임용 메시를 만듭니다. 학습 사진이 남아 있으면 표면 품질이 좋아집니다."),
    DE("Erstellt ein Mesh für Blender, Druck oder Spiele aus dem trainierten Modell. Vorhandene Trainingsfotos verbessern die Oberfläche."),
    FR("Crée un maillage pour Blender, l’impression ou les jeux. Les photos d’entraînement disponibles améliorent la surface."),
    ES("Crea una malla del modelo entrenado para Blender, impresión o juegos. Las fotos de entrenamiento mejoran la superficie si están disponibles."),
    PT("Cria uma malha do modelo treinado para Blender, impressão ou jogos. As fotos de treino disponíveis melhoram a superfície."),
    IT("Crea una mesh dal modello addestrato per Blender, stampa o giochi. Le foto di addestramento disponibili migliorano la superficie."),
    NL("Maak een mesh van een getraind model voor Blender, printen of games. Beschikbare trainingsfoto’s verbeteren het oppervlak."),
    RU("Создаёт сетку обученной модели для Blender, печати или игр. Сохранившиеся обучающие фото улучшают поверхность."),
    TR("Eğitilmiş modelden Blender, baskı veya oyun için ağ oluşturur. Mevcut eğitim fotoğrafları yüzeyi iyileştirir."));

SS_MSG(viewport_shading,
    EN("Shading"),
    JA("陰影"),
    ZH_HANS("明暗"),
    ZH_HANT("明暗"),
    KO("음영"),
    DE("Schattierung"),
    FR("Ombrage"),
    ES("Sombreado"),
    PT("Sombreamento"),
    IT("Ombreggiatura"),
    NL("Schaduw"),
    RU("Затенение"),
    TR("Gölgeleme"));

SS_MSG(viewport_flat_shading,
    EN("Flat"),
    JA("フラット"),
    ZH_HANS("平面"),
    ZH_HANT("平面"),
    KO("평면"),
    DE("Flach"),
    FR("Plat"),
    ES("Plano"),
    PT("Plano"),
    IT("Piatto"),
    NL("Vlak"),
    RU("Плоское"),
    TR("Düz"));

SS_MSG(viewport_flat_shading_help,
    EN("Light each triangle by its own normal, so the individual faces show."),
    JA("三角形ごとの法線で陰影を付けます。面のひとつひとつが見えます。"),
    ZH_HANS("按每个三角形自己的法线上光，可以看清一个个的面。"),
    ZH_HANT("按每個三角形自己的法線上光，可以看清一個個的面。"),
    KO("삼각형마다 자기 법선으로 빛을 줍니다. 면 하나하나가 보입니다."),
    DE("Beleuchtet jedes Dreieck mit seiner eigenen Normalen, sodass die "
       "einzelnen Flächen sichtbar werden."),
    FR("Éclaire chaque triangle avec sa propre normale : les facettes "
       "deviennent visibles."),
    ES("Ilumina cada triángulo con su propia normal, así se ven las caras una "
       "a una."),
    PT("Ilumina cada triângulo com a sua própria normal, então as faces "
       "aparecem uma a uma."),
    IT("Illumina ogni triangolo con la sua normale, così si vedono le singole "
       "facce."),
    NL("Belicht elke driehoek met zijn eigen normaal, zodat de losse vlakken "
       "zichtbaar worden."),
    RU("Освещает каждый треугольник по его собственной нормали, так что видны "
       "отдельные грани."),
    TR("Her üçgeni kendi normaliyle aydınlatır, böylece yüzler tek tek "
       "görünür."));

SS_MSG(viewport_mesh_color,
    EN("Color"),
    JA("色"),
    ZH_HANS("颜色"),
    ZH_HANT("顏色"),
    KO("색"),
    DE("Farbe"),
    FR("Couleur"),
    ES("Color"),
    PT("Cor"),
    IT("Colore"),
    NL("Kleur"),
    RU("Цвет"),
    TR("Renk"));

SS_MSG(viewport_mesh_color_help,
    EN("Show the mesh's own color. Off leaves plain grey, which is the best "
       "way to look at the shape."),
    JA("メッシュ自身の色を表示します。オフにすると無地のグレーになり、形を"
       "見るのに一番向いています。"),
    ZH_HANS("显示网格自带的颜色。关掉就是纯灰色，最适合看形状。"),
    ZH_HANT("顯示網格自帶的顏色。關掉就是純灰色，最適合看形狀。"),
    KO("메시 자체의 색을 보여 줍니다. 끄면 단색 회색이 되어 모양을 보기에 가장 "
       "좋습니다."),
    DE("Zeigt die eigene Farbe des Netzes. Aus bleibt schlichtes Grau -- am "
       "besten geeignet, um die Form zu beurteilen."),
    FR("Affiche la couleur propre du maillage. Désactivé, tout reste gris "
       "uni, ce qui est le mieux pour juger la forme."),
    ES("Muestra el color propio de la malla. Desactivado queda gris liso, que "
       "es lo mejor para ver la forma."),
    PT("Mostra a cor própria da malha. Desligado fica cinza liso, que é o "
       "melhor para ver a forma."),
    IT("Mostra il colore proprio della mesh. Spento resta grigio uniforme, "
       "che è il modo migliore per guardare la forma."),
    NL("Toont de eigen kleur van de mesh. Uit blijft het effen grijs, en dat "
       "is het beste om de vorm te bekijken."),
    RU("Показывает собственный цвет меша. Выключено -- ровный серый, на "
       "котором форма читается лучше всего."),
    TR("Ağın kendi rengini gösterir. Kapalıyken düz gri kalır; biçimi "
       "incelemek için en iyisi budur."));

SS_MSG(viewport_primitive_help,
    EN("3DGS: standard drawing. Mip: antialiasing. 3DGUT: ray-based drawing for fisheye and 360 views."),
    JA("3DGS：標準描画。Mip：アンチエイリアス。3DGUT：魚眼や360度表示用のレイに基づく描画。"),
    ZH_HANS("3DGS：标准绘制。Mip：抗锯齿。3DGUT：用于鱼眼和 360 视图的射线绘制。"),
    ZH_HANT("3DGS：標準繪製。Mip：反鋸齒。3DGUT：用於魚眼和 360 視圖的射線繪製。"),
    KO("3DGS: 표준 렌더링. Mip: 안티앨리어싱. 3DGUT: 어안 및 360 뷰용 광선 기반 렌더링."),
    DE("3DGS: Standarddarstellung. Mip: Kantenglättung. 3DGUT: strahlbasierte Darstellung für Fischauge und 360°."),
    FR("3DGS : rendu standard. Mip : anticrénelage. 3DGUT : rendu par rayons pour fisheye et vues à 360°."),
    ES("3DGS: dibujo estándar. Mip: suavizado. 3DGUT: dibujo por rayos para ojo de pez y vistas de 360°."),
    PT("3DGS: desenho padrão. Mip: antisserrilhamento. 3DGUT: desenho por raios para olho de peixe e vistas de 360°."),
    IT("3DGS: disegno standard. Mip: antialiasing. 3DGUT: disegno a raggi per fisheye e viste a 360°."),
    NL("3DGS: standaardweergave. Mip: antialiasing. 3DGUT: stralenweergave voor fisheye en 360°-beelden."),
    RU("3DGS: обычная отрисовка. Mip: сглаживание. 3DGUT: лучевая отрисовка для рыбьего глаза и видов 360°."),
    TR("3DGS: standart çizim. Mip: kenar yumuşatma. 3DGUT: balıkgözü ve 360° görünümler için ışın tabanlı çizim."));

SS_MSG(viewport_sh_degree,
    EN("SH"),
    JA("SH"),
    ZH_HANS("SH"),
    ZH_HANT("SH"),
    KO("SH"),
    DE("SH"),
    FR("SH"),
    ES("SH"),
    PT("SH"),
    IT("SH"),
    NL("SH"),
    RU("SH"),
    TR("SH"));

SS_MSG(viewport_sh_degree_help,
    EN("Spherical-harmonic bands to use. 0 is the flat base color; higher "
       "bands add the view-dependent shine the model learned."),
    JA("使う球面調和関数の次数です。0は下地の色だけ、次数を上げると学習した"
       "見る角度による艶が加わります。"),
    ZH_HANS("用到第几阶球谐。0 只有底色；阶数越高，越能加上模型学到的随视角变化"
            "的高光。"),
    ZH_HANT("用到第幾階球諧。0 只有底色；階數越高，越能加上模型學到的隨視角變化"
            "的高光。"),
    KO("사용할 구면조화 차수입니다. 0은 바탕색만이고, 차수를 올리면 모델이 배운 "
       "시점에 따른 광택이 더해집니다."),
    DE("Wie viele Kugelflächenfunktions-Bänder verwendet werden. 0 ist die "
       "flache Grundfarbe; höhere Bänder fügen den gelernten "
       "blickwinkelabhängigen Glanz hinzu."),
    FR("Nombre de bandes d'harmoniques sphériques utilisées. 0 donne la "
       "couleur de base ; les bandes supérieures ajoutent les reflets "
       "dépendant du point de vue que le modèle a appris."),
    ES("Cuántas bandas de armónicos esféricos se usan. 0 es el color base "
       "plano; las bandas altas añaden el brillo según el ángulo que el "
       "modelo aprendió."),
    PT("Quantas bandas de harmônicos esféricos usar. 0 é a cor base plana; as "
       "bandas altas acrescentam o brilho conforme o ângulo que o modelo "
       "aprendeu."),
    IT("Quante bande di armoniche sferiche usare. 0 è il colore di base "
       "piatto; le bande più alte aggiungono i riflessi che dipendono dal "
       "punto di vista appresi dal modello."),
    NL("Hoeveel banden sferische harmonischen worden gebruikt. 0 is de vlakke "
       "basiskleur; hogere banden voegen de aangeleerde glans toe die van de "
       "kijkhoek afhangt."),
    RU("Сколько полос сферических гармоник использовать. 0 -- плоский базовый "
       "цвет; старшие полосы добавляют выученный блик, зависящий от угла "
       "обзора."),
    TR("Kaç küresel harmonik bandının kullanılacağı. 0 düz taban rengidir; "
       "üst bantlar modelin öğrendiği, bakış açısına bağlı parlaklığı ekler."));

SS_MSG(viewport_gamut,
    EN("Gamut"),
    JA("色域"),
    ZH_HANS("色域"),
    ZH_HANT("色域"),
    KO("색역"),
    DE("Farbraum"),
    FR("Gamut"),
    ES("Gama"),
    PT("Gama"),
    IT("Gamut"),
    NL("Gamut"),
    RU("Гамма"),
    TR("Renk gamı"));

SS_MSG(viewport_gamut_help,
    EN("The gamut the model's values are in. Set it to what the run was "
       "trained with; the render is converted to Rec.709 for the screen."),
    JA("モデルの値がどの色域かです。学習時の設定に合わせてください。表示用に"
       "Rec.709へ変換されます。"),
    ZH_HANS("模型数值所处的色域。设成训练时用的那个；显示前会转成 Rec.709。"),
    ZH_HANT("模型數值所處的色域。設成訓練時用的那個；顯示前會轉成 Rec.709。"),
    KO("모델 값이 어느 색역인지입니다. 학습할 때 쓴 것으로 맞추세요. 화면용 "
       "Rec.709로 변환됩니다."),
    DE("Der Farbumfang, in dem die Werte des Modells liegen. Stellen Sie ihn "
       "auf das ein, womit der Lauf trainiert wurde; für den Bildschirm wird "
       "nach Rec.709 konvertiert."),
    FR("Le gamut des valeurs du modèle. Réglez-le sur celui de "
       "l'entraînement ; le rendu est converti en Rec.709 pour l'écran."),
    ES("La gama de color en la que están los valores del modelo. Ponla como "
       "se entrenó; el render se convierte a Rec.709 para la pantalla."),
    PT("A gama de cor em que estão os valores do modelo. Coloque como foi "
       "treinado; o render é convertido para Rec.709 na tela."),
    IT("Il gamut in cui stanno i valori del modello. Impostalo come "
       "l'addestramento; il render viene convertito in Rec.709 per lo schermo."),
    NL("Het gamut waarin de waarden van het model staan. Zet het op waarmee "
       "de run getraind is; de render wordt voor het scherm naar Rec.709 "
       "omgezet."),
    RU("Цветовой охват значений модели. Поставьте тот, с которым шло "
       "обучение; для экрана рендер переводится в Rec.709."),
    TR("Modelin değerlerinin bulunduğu renk gamı. Eğitimde ne kullanıldıysa "
       "onu seçin; render ekran için Rec.709'a çevrilir."));

SS_MSG(viewport_linear_color,
    EN("Linear"),
    JA("リニア"),
    ZH_HANS("线性"),
    ZH_HANT("線性"),
    KO("선형"),
    DE("Linear"),
    FR("Linéaire"),
    ES("Lineal"),
    PT("Linear"),
    IT("Lineare"),
    NL("Lineair"),
    RU("Линейно"),
    TR("Doğrusal"));

SS_MSG(viewport_linear_help,
    EN("The model's values are linear light rather than display values. Set it "
       "to what the run was trained with (--splat-color-is-linear)."),
    JA("モデルの値が表示値ではなくリニア光だという指定です。学習時の設定"
       "（--splat-color-is-linear）に合わせてください。"),
    ZH_HANS("模型数值是线性光，而不是显示值。设成训练时用的那个"
            "（--splat-color-is-linear）。"),
    ZH_HANT("模型數值是線性光，而不是顯示值。設成訓練時用的那個"
            "（--splat-color-is-linear）。"),
    KO("모델 값이 디스플레이 값이 아니라 선형 광량이라는 뜻입니다. 학습할 때 쓴 "
       "것(--splat-color-is-linear)으로 맞추세요."),
    DE("Die Werte des Modells sind lineares Licht statt Anzeigewerte. Auf das "
       "einstellen, womit der Lauf trainiert wurde (--splat-color-is-linear)."),
    FR("Les valeurs du modèle sont de la lumière linéaire et non des valeurs "
       "d'affichage. Réglez-le sur celui de l'entraînement "
       "(--splat-color-is-linear)."),
    ES("Los valores del modelo son luz lineal y no valores de pantalla. Ponlo "
       "como se entrenó (--splat-color-is-linear)."),
    PT("Os valores do modelo são luz linear e não valores de exibição. Coloque "
       "como foi treinado (--splat-color-is-linear)."),
    IT("I valori del modello sono luce lineare e non valori di visualizzazione. "
       "Impostalo come l'addestramento (--splat-color-is-linear)."),
    NL("De waarden van het model zijn lineair licht in plaats van "
       "weergavewaarden. Zet het op waarmee de run getraind is "
       "(--splat-color-is-linear)."),
    RU("Значения модели -- линейный свет, а не экранные значения. Поставьте то, "
       "с которым шло обучение (--splat-color-is-linear)."),
    TR("Modelin değerleri ekran değeri değil doğrusal ışıktır. Eğitimde ne "
       "kullanıldıysa onu seçin (--splat-color-is-linear)."));

SS_MSG(viewport_transfer_help,
    EN("The curve the model's values leave through on their way to the screen.\n- `aces`, `filmic` and `uncharted2` roll the highlights off instead of clipping them.\n- Set it to what the run was trained with (--splat-color-transfer)."),
    JA("モデルの値が画面へ出ていくときに通るカーブです。aces・filmic・uncharted2 はハイライトを切り捨てずになだらかに丸めます。学習時の設定（--splat-color-transfer）に合わせてください。"),
    ZH_HANS("模型数值送到屏幕时经过的曲线。aces、filmic、uncharted2 会把高光平滑压下来而不是直接截断。设成训练时用的那个（--splat-color-transfer）。"),
    ZH_HANT("模型數值送到螢幕時經過的曲線。aces、filmic、uncharted2 會把高光平滑壓下來而不是直接截斷。設成訓練時用的那個（--splat-color-transfer）。"),
    KO("모델 값이 화면으로 나갈 때 지나는 곡선입니다. aces, filmic, uncharted2는 밝은 부분을 잘라내지 않고 완만하게 눌러 줍니다. 학습할 때 쓴 것(--splat-color-transfer)으로 맞추세요."),
    DE("Die Kurve, über die die Werte des Modells zum Bildschirm gehen.\n- `aces`, `filmic` und `uncharted2` rollen die Lichter ab, statt sie abzuschneiden.\n- Auf den Trainingswert einstellen (--splat-color-transfer)."),
    FR("La courbe par laquelle les valeurs du modèle sortent vers l'écran.\n- « aces », « filmic » et « uncharted2 » adoucissent les hautes lumières au lieu de les écrêter.\n- Réglez-le sur celui de l'entraînement (--splat-color-transfer)."),
    ES("La curva por la que los valores del modelo salen hacia la pantalla.\n- «aces», «filmic» y «uncharted2» suavizan las altas luces en vez de recortarlas.\n- Ponlo como se entrenó (--splat-color-transfer)."),
    PT("A curva por que os valores do modelo saem para a tela.\n- «aces», «filmic» e «uncharted2» suavizam as altas luzes em vez de as cortar.\n- Coloque como foi treinado (--splat-color-transfer)."),
    IT("La curva da cui i valori del modello escono verso lo schermo.\n- «aces», «filmic» e «uncharted2» addolciscono le alte luci invece di troncarle.\n- Impostalo come l'addestramento (--splat-color-transfer)."),
    NL("De kromme waarlangs de waarden van het model naar het scherm gaan.\n- `aces`, `filmic` en `uncharted2` laten de hoge lichten aflopen in plaats van ze af te kappen.\n- Zet het op waarmee de run getraind is (--splat-color-transfer)."),
    RU("Кривая, через которую значения модели уходят на экран.\n- «aces», «filmic» и «uncharted2» плавно сводят света вместо обрезки.\n- Поставьте то, с которым шло обучение (--splat-color-transfer)."),
    TR("Modelin değerlerinin ekrana çıkarken geçtiği eğri.\n- `aces`, `filmic` ve `uncharted2` parlak bölgeleri kırpmak yerine yumuşatarak indirir.\n- Eğitimde ne kullanıldıysa onu seçin (--splat-color-transfer)."));

SS_MSG(viewport_gamut_none,
    EN("Rec.709 / sRGB"),
    JA("Rec.709 / sRGB"),
    ZH_HANS("Rec.709 / sRGB"),
    ZH_HANT("Rec.709 / sRGB"),
    KO("Rec.709 / sRGB"),
    DE("Rec.709 / sRGB"),
    FR("Rec.709 / sRGB"),
    ES("Rec.709 / sRGB"),
    PT("Rec.709 / sRGB"),
    IT("Rec.709 / sRGB"),
    NL("Rec.709 / sRGB"),
    RU("Rec.709 / sRGB"),
    TR("Rec.709 / sRGB"));

SS_MSG(mesh_drop_hint,
    EN("...or drop the model, or the photo folder, anywhere in this window"),
    JA("…または、モデルや写真フォルダをこのウィンドウのどこかにドロップして"
       "ください"),
    ZH_HANS("…或者把模型或照片文件夹拖到这个窗口的任意位置"),
    ZH_HANT("…或者把模型或相片資料夾拖到這個視窗的任意位置"),
    KO("…또는 모델이나 사진 폴더를 이 창 아무 곳에나 끌어다 놓으세요"),
    DE("… oder ziehen Sie das Modell oder den Fotoordner irgendwo in dieses "
       "Fenster"),
    FR("… ou déposez le modèle, ou le dossier de photos, n'importe où dans "
       "cette fenêtre"),
    ES("… o arrastre el modelo, o la carpeta de fotos, a cualquier punto de "
       "esta ventana"),
    PT("… ou arraste o modelo, ou a pasta de fotos, para qualquer ponto desta "
       "janela"),
    IT("… oppure trascina il modello, o la cartella delle foto, in un punto "
       "qualsiasi di questa finestra"),
    NL("… of sleep het model, of de fotomap, ergens in dit venster"),
    RU("…или перетащите модель либо папку с фотографиями в любое место этого "
       "окна"),
    TR("…ya da modeli veya fotoğraf klasörünü bu pencerenin herhangi bir "
       "yerine bırakın"));

SS_MSG(mesh_title,
    EN("Create a mesh"),
    JA("メッシュを作る"),
    ZH_HANS("生成网格"),
    ZH_HANT("產生網格"),
    KO("메시 만들기"),
    DE("Netz erzeugen"),
    FR("Créer un maillage"),
    ES("Crear una malla"),
    PT("Criar uma malha"),
    IT("Crea una mesh"),
    NL("Mesh maken"),
    RU("Построить меш"),
    TR("Ağ oluştur"));

SS_MSG(mesh_source,
    EN("Trained model"),
    JA("学習済みモデル"),
    ZH_HANS("训练好的模型"),
    ZH_HANT("訓練好的模型"),
    KO("학습한 모델"),
    DE("Trainiertes Modell"),
    FR("Modèle entraîné"),
    ES("Modelo entrenado"),
    PT("Modelo treinado"),
    IT("Modello addestrato"),
    NL("Getraind model"),
    RU("Обученная модель"),
    TR("Eğitilmiş model"));

SS_MSG(mesh_source_help,
    EN("A run folder, a step-*.ckpt folder, or a splat .ply file."),
    JA("実行フォルダ、step-*.ckpt フォルダ、またはスプラットの .ply ファイル。"),
    ZH_HANS("一个运行文件夹、一个 step-*.ckpt 文件夹，或者一个高斯点 .ply 文件。"),
    ZH_HANT("一個執行資料夾、一個 step-*.ckpt 資料夾，或者一個高斯點 .ply 檔案。"),
    KO("실행 폴더, step-*.ckpt 폴더, 또는 스플랫 .ply 파일."),
    DE("Ein Lauf-Ordner, ein step-*.ckpt-Ordner oder eine Splat-.ply-Datei."),
    FR("Un dossier de run, un dossier step-*.ckpt, ou un fichier .ply de splats."),
    ES("Una carpeta de ejecución, una carpeta step-*.ckpt o un archivo .ply de splats."),
    PT("Uma pasta de execução, uma pasta step-*.ckpt ou um arquivo .ply de splats."),
    IT("Una cartella di run, una cartella step-*.ckpt o un file .ply di splat."),
    NL("Een run-map, een step-*.ckpt-map of een splat-.ply-bestand."),
    RU("Папка запуска, папка step-*.ckpt или файл .ply со сплатами."),
    TR("Bir çalıştırma klasörü, bir step-*.ckpt klasörü ya da bir splat .ply dosyası."));

SS_MSG(mesh_use_photos,
    EN("Use the training photos"),
    JA("学習に使った写真を使う"),
    ZH_HANS("使用训练用的照片"),
    ZH_HANT("使用訓練用的相片"),
    KO("학습에 쓴 사진 사용"),
    DE("Die Trainingsfotos verwenden"),
    FR("Utiliser les photos d'entraînement"),
    ES("Usar las fotos de entrenamiento"),
    PT("Usar as fotos de treinamento"),
    IT("Usa le foto di addestramento"),
    NL("De trainingsfoto's gebruiken"),
    RU("Использовать фотографии обучения"),
    TR("Eğitim fotoğraflarını kullan"));

SS_MSG(mesh_use_photos_help,
    EN("The surface is carved from what the cameras actually saw, which removes interior fog and gives much better color. Turn this off to mesh from the Gaussians alone."),
    JA("カメラが実際に見たものから面を削り出します。内部のもやが消え、色もずっと良くなります。オフにするとガウシアンだけからメッシュを作ります。"),
    ZH_HANS("用相机真正看到的内容来雕出表面，可以去掉内部的雾，颜色也好很多。关掉的话就只用高斯本身生成网格。"),
    ZH_HANT("用相機真正看到的內容來雕出表面，可以去掉內部的霧，顏色也好很多。關掉的話就只用高斯本身產生網格。"),
    KO("카메라가 실제로 본 것에서 표면을 깎아냅니다. 내부의 안개가 사라지고 색도 훨씬 좋아집니다. 끄면 가우시안만으로 메시를 만듭니다."),
    DE("Formt die Oberfläche nach den Fotos: weniger innerer Nebel und bessere Farben. Aus erzeugt das Mesh nur aus Gauß-Verteilungen."),
    FR("Utilise les photos pour réduire la brume interne et améliorer les couleurs. Désactiver pour mailler les gaussiennes seules."),
    ES("La superficie se talla a partir de lo que las cámaras vieron de verdad: quita la niebla interior y da mucho mejor color. Desactívalo para mallar solo con las gaussianas."),
    PT("A superfície é esculpida a partir do que as câmeras realmente viram: tira a névoa interna e dá cores bem melhores. Desligue para gerar a malha só com as gaussianas."),
    IT("La superficie viene scavata da ciò che le fotocamere hanno visto davvero: niente nebbia interna e colori molto migliori. Disattiva per creare la mesh dalle sole gaussiane."),
    NL("Het oppervlak wordt uitgesneden uit wat de camera's echt zagen: geen mist binnenin en veel betere kleuren. Zet dit uit om alleen uit de Gaussianen een mesh te maken."),
    RU("Поверхность вырезается по тому, что действительно видели камеры: исчезает внутренний туман и цвет получается заметно лучше. Выключите, чтобы строить меш только по гауссианам."),
    TR("Yüzey, kameraların gerçekten gördüğünden oyulur: içerideki sis gider ve renk çok daha iyi olur. Yalnızca gauss'lardan ağ oluşturmak için kapatın."));

SS_MSG(mesh_photos_dir,
    EN("Photo folder"),
    JA("写真フォルダ"),
    ZH_HANS("照片文件夹"),
    ZH_HANT("相片資料夾"),
    KO("사진 폴더"),
    DE("Fotoordner"),
    FR("Dossier de photos"),
    ES("Carpeta de fotos"),
    PT("Pasta de fotos"),
    IT("Cartella delle foto"),
    NL("Fotomap"),
    RU("Папка с фотографиями"),
    TR("Fotoğraf klasörü"));

SS_MSG(mesh_photos_dir_help,
    EN("Leave empty to use the folder recorded in the run's config.json."),
    JA("空にしておくと、実行の config.json に記録されたフォルダを使います。"),
    ZH_HANS("留空则使用运行的 config.json 里记录的文件夹。"),
    ZH_HANT("留空則使用執行的 config.json 裡記錄的資料夾。"),
    KO("비워 두면 실행의 config.json에 기록된 폴더를 사용합니다."),
    DE("Leer lassen, um den in der config.json des Laufs vermerkten Ordner zu nehmen."),
    FR("Laissez vide pour utiliser le dossier noté dans le config.json du run."),
    ES("Déjalo vacío para usar la carpeta anotada en el config.json de la ejecución."),
    PT("Deixe vazio para usar a pasta anotada no config.json da execução."),
    IT("Lascia vuoto per usare la cartella indicata nel config.json del run."),
    NL("Laat leeg om de map te gebruiken die in de config.json van de run staat."),
    RU("Оставьте пустым, чтобы взять папку из config.json запуска."),
    TR("Çalıştırmanın config.json dosyasındaki klasörü kullanmak için boş bırakın."));

// The two warnings the mesh screen shows when the run about to start would
// have no cameras -- the checkbox is off, or nothing was found to use. Both
// say the same thing the CLI says (i18n/catalog/Cli.h, mesh_no_cameras): a
// mesh carved from the photos is a much better mesh, and this is the moment
// to say so, before three minutes of work produce the worse one.
SS_MSG(mesh_no_photos_warn,
    EN("Without the photos the mesh comes from the Gaussian densities alone: "
       "the surface is rougher and the colors are worse. Turn this on for "
       "much better quality."),
    JA("写真を使わないと、メッシュはガウシアンの密度だけから作られ、面は粗く、"
       "色も悪くなります。オンにすると品質が大きく上がります。"),
    ZH_HANS("不用照片时，网格只根据高斯密度生成：表面更粗糙，颜色也更差。"
            "打开它可以让质量好很多。"),
    ZH_HANT("不用相片時，網格只根據高斯密度產生：表面更粗糙，顏色也更差。"
            "打開它可以讓品質好很多。"),
    KO("사진을 쓰지 않으면 메시가 가우시안 밀도만으로 만들어져 표면이 거칠고 "
       "색도 나빠집니다. 켜면 품질이 훨씬 좋아집니다."),
    DE("Ohne die Fotos entsteht das Netz nur aus den Gauß-Dichten: die "
       "Oberfläche wird rauer und die Farben schlechter. Einschalten für "
       "deutlich bessere Qualität."),
    FR("Sans les photos, le maillage ne vient que des densités gaussiennes : "
       "la surface est plus grossière et les couleurs moins bonnes. Activez "
       "pour une qualité bien meilleure."),
    ES("Sin las fotos la malla sale solo de las densidades gaussianas: la "
       "superficie queda más basta y el color peor. Actívalo para una calidad "
       "bastante mejor."),
    PT("Sem as fotos a malha vem só das densidades gaussianas: a superfície "
       "fica mais grosseira e as cores piores. Ligue para uma qualidade bem "
       "melhor."),
    IT("Senza le foto la mesh viene solo dalle densità gaussiane: la "
       "superficie è più grezza e i colori peggiori. Attivalo per una qualità "
       "molto migliore."),
    NL("Zonder de foto's komt de mesh alleen uit de Gauss-dichtheden: het "
       "oppervlak wordt ruwer en de kleuren slechter. Zet dit aan voor "
       "duidelijk betere kwaliteit."),
    RU("Без фотографий меш строится только по гауссовым плотностям: "
       "поверхность грубее, а цвета хуже. Включите — качество будет заметно "
       "лучше."),
    TR("Fotoğraflar olmadan ağ yalnızca Gauss yoğunluklarından çıkar: yüzey "
       "daha kaba, renkler daha kötü olur. Belirgin biçimde daha iyi kalite "
       "için açın."));

SS_MSG(mesh_photos_missing_warn,
    EN("No photo folder found for this model, so the mesh will come from the "
       "Gaussian densities alone. Fill in the folder above for a much better "
       "surface."),
    JA("このモデルの写真フォルダが見つかりません。このままではガウシアンの密度"
       "だけからメッシュを作ります。上でフォルダを指定すると、面がずっと良く"
       "なります。"),
    ZH_HANS("找不到这个模型的照片文件夹，网格将只根据高斯密度生成。"
            "在上面填入文件夹，表面会好很多。"),
    ZH_HANT("找不到這個模型的相片資料夾，網格將只根據高斯密度產生。"
            "在上面填入資料夾，表面會好很多。"),
    KO("이 모델의 사진 폴더를 찾지 못했습니다. 이대로면 가우시안 밀도만으로 "
       "메시를 만듭니다. 위에 폴더를 넣으면 표면이 훨씬 좋아집니다."),
    DE("Für dieses Modell wurde kein Fotoordner gefunden, das Netz entsteht "
       "also nur aus den Gauß-Dichten. Oben einen Ordner eintragen, dann wird "
       "die Oberfläche deutlich besser."),
    FR("Aucun dossier de photos trouvé pour ce modèle : le maillage ne "
       "viendra que des densités gaussiennes. Indiquez le dossier ci-dessus "
       "pour une surface bien meilleure."),
    ES("No se encontró ninguna carpeta de fotos para este modelo, así que la "
       "malla saldrá solo de las densidades gaussianas. Indica la carpeta "
       "arriba para una superficie bastante mejor."),
    PT("Nenhuma pasta de fotos encontrada para este modelo, então a malha "
       "virá só das densidades gaussianas. Preencha a pasta acima para uma "
       "superfície bem melhor."),
    IT("Nessuna cartella di foto trovata per questo modello: la mesh verrà "
       "solo dalle densità gaussiane. Indica la cartella qui sopra per una "
       "superficie molto migliore."),
    NL("Geen fotomap gevonden voor dit model, dus de mesh komt alleen uit de "
       "Gauss-dichtheden. Vul hierboven de map in voor een duidelijk beter "
       "oppervlak."),
    RU("Папка с фотографиями для этой модели не найдена, поэтому меш будет "
       "построен только по гауссовым плотностям. Укажите папку выше — "
       "поверхность будет заметно лучше."),
    TR("Bu model için fotoğraf klasörü bulunamadı, bu yüzden ağ yalnızca "
       "Gauss yoğunluklarından çıkacak. Yukarıya klasörü girerseniz yüzey "
       "belirgin biçimde iyileşir."));

SS_MSG(mesh_color,
    EN("Color"),
    JA("色"),
    ZH_HANS("颜色"),
    ZH_HANT("顏色"),
    KO("색"),
    DE("Farbe"),
    FR("Couleur"),
    ES("Color"),
    PT("Cor"),
    IT("Colore"),
    NL("Kleur"),
    RU("Цвет"),
    TR("Renk"));

SS_MSG(mesh_color_none,
    EN("None (shape only)"),
    JA("なし（形だけ）"),
    ZH_HANS("无（只有形状）"),
    ZH_HANT("無（只有形狀）"),
    KO("없음(모양만)"),
    DE("Keine (nur Form)"),
    FR("Aucune (forme seule)"),
    ES("Ninguno (solo la forma)"),
    PT("Nenhuma (só a forma)"),
    IT("Nessuno (solo la forma)"),
    NL("Geen (alleen de vorm)"),
    RU("Нет (только форма)"),
    TR("Yok (yalnızca biçim)"));

SS_MSG(mesh_color_vertex,
    EN("Per-vertex color"),
    JA("頂点ごとの色"),
    ZH_HANS("逐顶点颜色"),
    ZH_HANT("逐頂點顏色"),
    KO("정점별 색"),
    DE("Farbe pro Eckpunkt"),
    FR("Couleur par sommet"),
    ES("Color por vértice"),
    PT("Cor por vértice"),
    IT("Colore per vertice"),
    NL("Kleur per hoekpunt"),
    RU("Цвет в вершинах"),
    TR("Köşe başına renk"));

SS_MSG(mesh_color_texture,
    EN("Baked texture"),
    JA("焼き込みテクスチャ"),
    ZH_HANS("烘焙贴图"),
    ZH_HANT("烘焙貼圖"),
    KO("구운 텍스처"),
    DE("Gebackene Textur"),
    FR("Texture cuite"),
    ES("Textura horneada"),
    PT("Textura assada"),
    IT("Texture cotta"),
    NL("Ingebakken textuur"),
    RU("Запечённая текстура"),
    TR("Pişirilmiş doku"));

SS_MSG(mesh_formats,
    EN("Save as"),
    JA("保存形式"),
    ZH_HANS("保存为"),
    ZH_HANT("儲存為"),
    KO("저장 형식"),
    DE("Speichern als"),
    FR("Enregistrer en"),
    ES("Guardar como"),
    PT("Salvar como"),
    IT("Salva come"),
    NL("Opslaan als"),
    RU("Сохранить как"),
    TR("Farklı kaydet"));

SS_MSG(mesh_output,
    EN("Output name"),
    JA("出力名"),
    ZH_HANS("输出名称"),
    ZH_HANT("輸出名稱"),
    KO("출력 이름"),
    DE("Ausgabename"),
    FR("Nom de sortie"),
    ES("Nombre de salida"),
    PT("Nome de saída"),
    IT("Nome di uscita"),
    NL("Uitvoernaam"),
    RU("Имя результата"),
    TR("Çıktı adı"));

SS_MSG(mesh_output_help,
    EN("Without an extension: one file per chosen format is written next to "
       "it. Leave empty to write beside the model."),
    JA("拡張子は付けません。選んだ形式ごとに1つのファイルが隣に書き出されます。"
       "空にするとモデルの隣に書き出します。"),
    ZH_HANS("不要带扩展名：每种选中的格式会各写一个文件放在旁边。留空就写在模型旁边。"),
    ZH_HANT("不要帶副檔名：每種選取的格式會各寫一個檔案放在旁邊。留空就寫在模型旁邊。"),
    KO("확장자 없이 적습니다. 고른 형식마다 파일이 하나씩 옆에 쓰입니다. 비워 두면 "
       "모델 옆에 씁니다."),
    DE("Ohne Endung: pro gewähltem Format wird eine Datei daneben geschrieben. "
       "Leer lassen, um neben dem Modell zu schreiben."),
    FR("Sans extension : un fichier par format choisi est écrit à côté. Laissez "
       "vide pour écrire à côté du modèle."),
    ES("Sin extensión: se escribe un archivo por cada formato elegido al lado. "
       "Déjalo vacío para escribir junto al modelo."),
    PT("Sem extensão: é escrito um arquivo por formato escolhido ao lado. Deixe "
       "vazio para escrever ao lado do modelo."),
    IT("Senza estensione: viene scritto un file per ogni formato scelto "
       "accanto. Lascia vuoto per scrivere accanto al modello."),
    NL("Zonder extensie: per gekozen formaat wordt er een bestand naast "
       "geschreven. Laat leeg om naast het model te schrijven."),
    RU("Без расширения: рядом появится по одному файлу на каждый выбранный "
       "формат. Оставьте пустым, чтобы записать рядом с моделью."),
    TR("Uzantısız: seçilen her biçim için yanına bir dosya yazılır. Modelin "
       "yanına yazmak için boş bırakın."));

SS_MSG(mesh_max_cameras,
    EN("Photos to use"),
    JA("使う写真の枚数"),
    ZH_HANS("使用的照片数"),
    ZH_HANT("使用的相片數"),
    KO("사용할 사진 수"),
    DE("Zu verwendende Fotos"),
    FR("Photos à utiliser"),
    ES("Fotos a usar"),
    PT("Fotos a usar"),
    IT("Foto da usare"),
    NL("Te gebruiken foto's"),
    RU("Сколько фотографий брать"),
    TR("Kullanılacak fotoğraflar"));

SS_MSG(mesh_max_cameras_help,
    EN("0 uses every photo, which is best but can be slow on a dataset with many of them. A smaller positive number picks a well-spread subset."),
    JA("0 ならすべての写真を使います。いちばん良い結果になりますが、写真の多いデータセットでは時間がかかります。正の小さい数にすると満遍なく散らばった一部だけを使います。"),
    ZH_HANS("0 表示用上全部照片，效果最好，但照片很多的数据集会比较慢。填一个较小的正数会挑一批分布均匀的照片。"),
    ZH_HANT("0 表示用上全部相片，效果最好，但相片很多的資料集會比較慢。填一個較小的正數會挑一批分布均勻的相片。"),
    KO("0이면 사진을 모두 씁니다. 결과는 가장 좋지만 사진이 많은 데이터셋에서는 느릴 수 있습니다. 작은 양수를 넣으면 고르게 퍼진 일부만 고릅니다."),
    DE("0 nutzt jedes Foto -- das beste Ergebnis, bei einem Datensatz mit vielen Fotos aber langsam. Eine kleinere positive Zahl wählt eine gut verteilte Teilmenge."),
    FR("0 utilise toutes les photos : meilleur résultat, mais plus lent. Une valeur positive plus petite choisit un sous-ensemble bien réparti."),
    ES("0 usa todas las fotos: es lo mejor, pero puede ser lento en un conjunto con muchas. Un número positivo menor elige un subconjunto bien repartido."),
    PT("0 usa todas as fotos: é o melhor, mas pode ser lento num conjunto com muitas. Um número positivo menor escolhe um subconjunto bem espalhado."),
    IT("0 usa tutte le foto: risultato migliore, ma più lento. Un valore positivo inferiore sceglie un sottoinsieme ben distribuito."),
    NL("0 gebruikt elke foto: dat is het best, maar bij een dataset met veel foto's kan het traag zijn. Een kleiner positief getal kiest een goed verspreide selectie."),
    RU("0 берёт все фотографии — результат лучший, но на наборе с большим их числом это медленно. Меньшее положительное число выбирает равномерно разбросанное подмножество."),
    TR("0 tüm fotoğrafları kullanır; en iyi sonuç budur ama fotoğrafı çok olan bir veri kümesinde yavaş olabilir. Daha küçük bir pozitif sayı iyi dağılmış bir alt küme seçer."));

SS_MSG(mesh_texture_size,
    EN("Texture size"),
    JA("テクスチャのサイズ"),
    ZH_HANS("贴图尺寸"),
    ZH_HANT("貼圖尺寸"),
    KO("텍스처 크기"),
    DE("Texturgröße"),
    FR("Taille de la texture"),
    ES("Tamaño de la textura"),
    PT("Tamanho da textura"),
    IT("Dimensione della texture"),
    NL("Textuurgrootte"),
    RU("Размер текстуры"),
    TR("Doku boyutu"));

SS_MSG(mesh_texture_size_auto,
    EN("Automatic"),
    JA("自動"),
    ZH_HANS("自动"),
    ZH_HANT("自動"),
    KO("자동"),
    DE("Automatisch"),
    FR("Automatique"),
    ES("Automático"),
    PT("Automático"),
    IT("Automatico"),
    NL("Automatisch"),
    RU("Автоматически"),
    TR("Otomatik"));

SS_MSG(mesh_advanced,
    EN("Advanced"),
    JA("詳細設定"),
    ZH_HANS("高级设置"),
    ZH_HANT("進階設定"),
    KO("고급 설정"),
    DE("Erweitert"),
    FR("Avancé"),
    ES("Avanzado"),
    PT("Avançado"),
    IT("Avanzate"),
    NL("Geavanceerd"),
    RU("Дополнительно"),
    TR("Gelişmiş"));

SS_MSG(mesh_detail,
    EN("Surface detail"),
    JA("面の細かさ"),
    ZH_HANS("表面细节"),
    ZH_HANT("表面細節"),
    KO("표면 세밀도"),
    DE("Oberflächendetail"),
    FR("Détail de la surface"),
    ES("Detalle de la superficie"),
    PT("Detalhe da superfície"),
    IT("Dettaglio della superficie"),
    NL("Oppervlaktedetail"),
    RU("Детальность поверхности"),
    TR("Yüzey ayrıntısı"));

SS_MSG(mesh_detail_help,
    EN("Lower keeps more triangles and more detail; higher merges short edges "
       "into a lighter mesh."),
    JA("小さいほど三角形が多く残り、細かくなります。大きいほど短い辺をまとめて"
       "軽いメッシュになります。"),
    ZH_HANS("越小保留的三角形越多、越细；越大就把短边合并掉，网格更轻。"),
    ZH_HANT("越小保留的三角形越多、越細；越大就把短邊合併掉，網格更輕。"),
    KO("작을수록 삼각형과 디테일이 더 남고, 클수록 짧은 변을 합쳐 가벼운 메시가 "
       "됩니다."),
    DE("Kleiner behält mehr Dreiecke und mehr Details; größer fasst kurze "
       "Kanten zu einem leichteren Netz zusammen."),
    FR("Plus bas garde plus de triangles et de détails ; plus haut fusionne les "
       "arêtes courtes en un maillage plus léger."),
    ES("Más bajo conserva más triángulos y más detalle; más alto fusiona las "
       "aristas cortas en una malla más ligera."),
    PT("Menor mantém mais triângulos e mais detalhe; maior junta as arestas "
       "curtas em uma malha mais leve."),
    IT("Più basso mantiene più triangoli e più dettaglio; più alto unisce gli "
       "spigoli corti in una mesh più leggera."),
    NL("Lager houdt meer driehoeken en meer detail; hoger voegt korte randen "
       "samen tot een lichtere mesh."),
    RU("Меньше -- больше треугольников и деталей; больше -- короткие рёбра "
       "сливаются, и меш становится легче."),
    TR("Daha düşük olması daha çok üçgen ve ayrıntı bırakır; daha yüksek olması "
       "kısa kenarları birleştirip daha hafif bir ağ yapar."));

SS_MSG(mesh_drop_specks,
    EN("Drop specks smaller than"),
    JA("これより小さいかけらを捨てる"),
    ZH_HANS("丢掉小于此值的碎片"),
    ZH_HANT("丟掉小於此值的碎片"),
    KO("이보다 작은 조각 버리기"),
    DE("Bruchstücke verwerfen, kleiner als"),
    FR("Jeter les fragments plus petits que"),
    ES("Descartar fragmentos menores que"),
    PT("Descartar fragmentos menores que"),
    IT("Scarta i frammenti più piccoli di"),
    NL("Snippers weggooien kleiner dan"),
    RU("Убирать обрывки меньше"),
    TR("Şundan küçük parçaları at"));

SS_MSG(mesh_cull_unseen,
    EN("Remove parts no photo saw"),
    JA("どの写真にも写っていない部分を消す"),
    ZH_HANS("删掉没有任何照片拍到的部分"),
    ZH_HANT("刪掉沒有任何相片拍到的部分"),
    KO("어떤 사진에도 없는 부분 지우기"),
    DE("Teile entfernen, die kein Foto gesehen hat"),
    FR("Supprimer ce qu'aucune photo n'a vu"),
    ES("Quitar las partes que ninguna foto vio"),
    PT("Remover as partes que nenhuma foto viu"),
    IT("Rimuovi le parti che nessuna foto ha visto"),
    NL("Delen verwijderen die geen enkele foto zag"),
    RU("Убрать участки, которых не видела ни одна фотография"),
    TR("Hiçbir fotoğrafın görmediği parçaları kaldır"));

SS_MSG(mesh_extra_args,
    EN("Extra arguments"),
    JA("追加の引数"),
    ZH_HANS("额外参数"),
    ZH_HANT("額外參數"),
    KO("추가 인자"),
    DE("Zusätzliche Argumente"),
    FR("Arguments supplémentaires"),
    ES("Argumentos adicionales"),
    PT("Argumentos adicionais"),
    IT("Argomenti aggiuntivi"),
    NL("Extra argumenten"),
    RU("Дополнительные аргументы"),
    TR("Ek argümanlar"));

SS_MSG(mesh_start,
    EN("Create the mesh"),
    JA("メッシュを作る"),
    ZH_HANS("开始生成网格"),
    ZH_HANT("開始產生網格"),
    KO("메시 만들기"),
    DE("Netz erzeugen"),
    FR("Créer le maillage"),
    ES("Crear la malla"),
    PT("Criar a malha"),
    IT("Crea la mesh"),
    NL("Mesh maken"),
    RU("Построить меш"),
    TR("Ağı oluştur"));

SS_MSG(mesh_cancel,
    EN("Stop"),
    JA("中止"),
    ZH_HANS("停止"),
    ZH_HANT("停止"),
    KO("중지"),
    DE("Anhalten"),
    FR("Arrêter"),
    ES("Detener"),
    PT("Parar"),
    IT("Ferma"),
    NL("Stoppen"),
    RU("Остановить"),
    TR("Durdur"));

SS_MSG(mesh_running,
    EN("Creating the mesh..."),
    JA("メッシュを作っています..."),
    ZH_HANS("正在生成网格..."),
    ZH_HANT("正在產生網格..."),
    KO("메시를 만드는 중..."),
    DE("Netz wird erzeugt..."),
    FR("Création du maillage..."),
    ES("Creando la malla..."),
    PT("Criando a malha..."),
    IT("Creazione della mesh..."),
    NL("Mesh wordt gemaakt..."),
    RU("Строится меш..."),
    TR("Ağ oluşturuluyor..."));

SS_MSG(mesh_done,
    EN("Done. Vertices: {0}   Triangles: {1}"),
    JA("完了。頂点: {0}   三角形: {1}"),
    ZH_HANS("完成。顶点: {0}   三角形: {1}"),
    ZH_HANT("完成。頂點: {0}   三角形: {1}"),
    KO("완료. 정점: {0}   삼각형: {1}"),
    DE("Fertig. Eckpunkte: {0}   Dreiecke: {1}"),
    FR("Terminé. Sommets : {0}   Triangles : {1}"),
    ES("Listo. Vértices: {0}   Triángulos: {1}"),
    PT("Pronto. Vértices: {0}   Triângulos: {1}"),
    IT("Fatto. Vertici: {0}   Triangoli: {1}"),
    NL("Klaar. Hoekpunten: {0}   Driehoeken: {1}"),
    RU("Готово. Вершин: {0}   Треугольников: {1}"),
    TR("Bitti. Köşe: {0}   Üçgen: {1}"));

SS_MSG(mesh_failed,
    EN("The mesh could not be created."),
    JA("メッシュを作れませんでした。"),
    ZH_HANS("没能生成网格。"),
    ZH_HANT("沒能產生網格。"),
    KO("메시를 만들지 못했습니다."),
    DE("Das Netz konnte nicht erzeugt werden."),
    FR("Le maillage n'a pas pu être créé."),
    ES("No se pudo crear la malla."),
    PT("Não foi possível criar a malha."),
    IT("Non è stato possibile creare la mesh."),
    NL("De mesh kon niet worden gemaakt."),
    RU("Не удалось построить меш."),
    TR("Ağ oluşturulamadı."));

SS_MSG(mesh_side_splats,
    EN("Splats"),
    JA("スプラット"),
    ZH_HANS("高斯点"),
    ZH_HANT("高斯點"),
    KO("스플랫"),
    DE("Splats"),
    FR("Splats"),
    ES("Splats"),
    PT("Splats"),
    IT("Splat"),
    NL("Splats"),
    RU("Сплаты"),
    TR("Splat'lar"));

SS_MSG(mesh_side_mesh,
    EN("Mesh"),
    JA("メッシュ"),
    ZH_HANS("网格"),
    ZH_HANT("網格"),
    KO("메시"),
    DE("Netz"),
    FR("Maillage"),
    ES("Malla"),
    PT("Malha"),
    IT("Mesh"),
    NL("Mesh"),
    RU("Меш"),
    TR("Ağ"));

SS_MSG(mesh_open_in_viewer,
    EN("Open the mesh on its own"),
    JA("メッシュだけを開く"),
    ZH_HANS("单独打开网格"),
    ZH_HANT("單獨開啟網格"),
    KO("메시만 따로 열기"),
    DE("Nur das Netz öffnen"),
    FR("Ouvrir le maillage seul"),
    ES("Abrir solo la malla"),
    PT("Abrir só a malha"),
    IT("Apri solo la mesh"),
    NL("Alleen de mesh openen"),
    RU("Открыть только меш"),
    TR("Yalnızca ağı aç"));

SS_MSG(mesh_pick_model,
    EN("Pick a trained model"),
    JA("学習済みモデルを選ぶ"),
    ZH_HANS("选择训练好的模型"),
    ZH_HANT("選擇訓練好的模型"),
    KO("학습한 모델 고르기"),
    DE("Trainiertes Modell auswählen"),
    FR("Choisir un modèle entraîné"),
    ES("Elegir un modelo entrenado"),
    PT("Escolher um modelo treinado"),
    IT("Scegli un modello addestrato"),
    NL("Kies een getraind model"),
    RU("Выберите обученную модель"),
    TR("Eğitilmiş bir model seçin"));

SS_MSG(mesh_pick_photos,
    EN("Pick the photo folder"),
    JA("写真フォルダを選ぶ"),
    ZH_HANS("选择照片文件夹"),
    ZH_HANT("選擇相片資料夾"),
    KO("사진 폴더 고르기"),
    DE("Fotoordner auswählen"),
    FR("Choisir le dossier de photos"),
    ES("Elegir la carpeta de fotos"),
    PT("Escolher a pasta de fotos"),
    IT("Scegli la cartella delle foto"),
    NL("Kies de fotomap"),
    RU("Выберите папку с фотографиями"),
    TR("Fotoğraf klasörünü seçin"));

SS_MSG(mesh_pick_output,
    EN("Pick where to save"),
    JA("保存先を選ぶ"),
    ZH_HANS("选择保存位置"),
    ZH_HANT("選擇儲存位置"),
    KO("저장할 곳 고르기"),
    DE("Speicherort auswählen"),
    FR("Choisir où enregistrer"),
    ES("Elegir dónde guardar"),
    PT("Escolher onde salvar"),
    IT("Scegli dove salvare"),
    NL("Kies waar op te slaan"),
    RU("Выберите, куда сохранить"),
    TR("Nereye kaydedileceğini seçin"));

SS_MSG(mesh_no_model,
    EN("Choose a trained model first."),
    JA("先に学習済みモデルを選んでください。"),
    ZH_HANS("请先选一个训练好的模型。"),
    ZH_HANT("請先選一個訓練好的模型。"),
    KO("먼저 학습한 모델을 고르세요."),
    DE("Wählen Sie zuerst ein trainiertes Modell."),
    FR("Choisissez d'abord un modèle entraîné."),
    ES("Elige primero un modelo entrenado."),
    PT("Escolha primeiro um modelo treinado."),
    IT("Scegli prima un modello addestrato."),
    NL("Kies eerst een getraind model."),
    RU("Сначала выберите обученную модель."),
    TR("Önce eğitilmiş bir model seçin."));

SS_MSG(viewer_loaded_mesh,
    EN("Loaded. Vertices: {0}   Triangles: {1}"),
    JA("読み込みました。頂点: {0}   三角形: {1}"),
    ZH_HANS("已载入。顶点: {0}   三角形: {1}"),
    ZH_HANT("已載入。頂點: {0}   三角形: {1}"),
    KO("불러왔습니다. 정점: {0}   삼각형: {1}"),
    DE("Geladen. Eckpunkte: {0}   Dreiecke: {1}"),
    FR("Chargé. Sommets : {0}   Triangles : {1}"),
    ES("Cargado. Vértices: {0}   Triángulos: {1}"),
    PT("Carregado. Vértices: {0}   Triângulos: {1}"),
    IT("Caricato. Vertici: {0}   Triangoli: {1}"),
    NL("Geladen. Hoekpunten: {0}   Driehoeken: {1}"),
    RU("Загружено. Вершин: {0}   Треугольников: {1}"),
    TR("Yüklendi. Köşe: {0}   Üçgen: {1}"));

SS_MSG(viewer_mesh_count,
    EN("Vertices: {0}   Triangles: {1}"),
    JA("頂点: {0}   三角形: {1}"),
    ZH_HANS("顶点: {0}   三角形: {1}"),
    ZH_HANT("頂點: {0}   三角形: {1}"),
    KO("정점: {0}   삼각형: {1}"),
    DE("Eckpunkte: {0}   Dreiecke: {1}"),
    FR("Sommets : {0}   Triangles : {1}"),
    ES("Vértices: {0}   Triángulos: {1}"),
    PT("Vértices: {0}   Triângulos: {1}"),
    IT("Vertici: {0}   Triangoli: {1}"),
    NL("Hoekpunten: {0}   Driehoeken: {1}"),
    RU("Вершин: {0}   Треугольников: {1}"),
    TR("Köşe: {0}   Üçgen: {1}"));


// The count drawn over the preview image. Labelled rather than inflected, so
// no language needs a plural rule for it, and short: it sits on the picture.
SS_MSG(overlay_triangles,
    EN("Triangles: {0}"),
    JA("三角形: {0}"),
    ZH_HANS("三角面：{0}"),
    ZH_HANT("三角面：{0}"),
    KO("삼각형: {0}"),
    DE("Dreiecke: {0}"),
    FR("Triangles : {0}"),
    ES("Triángulos: {0}"),
    PT("Triângulos: {0}"),
    IT("Triangoli: {0}"),
    NL("Driehoeken: {0}"),
    RU("Треугольники: {0}"),
    TR("Üçgen: {0}"));

SS_MSG(overlay_points,
    EN("Points: {0}"),
    JA("点: {0}"),
    ZH_HANS("点：{0}"),
    ZH_HANT("點：{0}"),
    KO("점: {0}"),
    DE("Punkte: {0}"),
    FR("Points : {0}"),
    ES("Puntos: {0}"),
    PT("Pontos: {0}"),
    IT("Punti: {0}"),
    NL("Punten: {0}"),
    RU("Точки: {0}"),
    TR("Nokta: {0}"));

SS_MSG(overlay_grid_metric,
    EN("Grid: {0}"),
    JA("グリッド: {0}"),
    ZH_HANS("网格：{0}"),
    ZH_HANT("格線：{0}"),
    KO("격자: {0}"),
    DE("Raster: {0}"),
    FR("Grille : {0}"),
    ES("Cuadrícula: {0}"),
    PT("Grelha: {0}"),
    IT("Griglia: {0}"),
    NL("Raster: {0}"),
    RU("Сетка: {0}"),
    TR("Izgara: {0}"));

SS_MSG(overlay_grid_relative,
    EN("Grid: {0} (no metric scale)"),
    JA("グリッド: {0}（実寸不明）"),
    ZH_HANS("网格：{0}（无实际尺度）"),
    ZH_HANT("格線：{0}（無實際尺度）"),
    KO("격자: {0} (실척 없음)"),
    DE("Raster: {0} (kein metrischer Maßstab)"),
    FR("Grille : {0} (pas d'échelle métrique)"),
    ES("Cuadrícula: {0} (sin escala métrica)"),
    PT("Grelha: {0} (sem escala métrica)"),
    IT("Griglia: {0} (nessuna scala metrica)"),
    NL("Raster: {0} (geen metrische schaal)"),
    RU("Сетка: {0} (масштаб не в метрах)"),
    TR("Izgara: {0} (metrik ölçek yok)"));


// ===========================================================================
// Photograph vs render (src/app/gui/ImageCompare.h)
// ===========================================================================

SS_MSG(compare_separate,
    EN("Separate images"), JA("画像を並べる"), ZH_HANS("分开显示"), ZH_HANT("分開顯示"),
    KO("이미지 나란히"), DE("Einzelbilder"), FR("Images séparées"), ES("Imágenes separadas"),
    PT("Imagens separadas"), IT("Immagini separate"), NL("Afzonderlijke beelden"),
    RU("Отдельные изображения"), TR("Ayrı görüntüler"));

SS_MSG(compare_blend,
    EN("Blend"), JA("重ね合わせ"), ZH_HANS("混合"), ZH_HANT("混合"),
    KO("혼합"), DE("Überblenden"), FR("Fondu"), ES("Mezcla"),
    PT("Mistura"), IT("Fusione"), NL("Overvloeien"), RU("Смешивание"), TR("Karıştır"));

SS_MSG(compare_separate_help,
    EN("Show the photograph and render side by side, with optional reference and rendered depth, normals, and the detail error map."),
    JA("写真とレンダリングを並べて表示します。参照とレンダリングの深度・法線、詳細の誤差マップも表示できます。"),
    ZH_HANS("并排显示照片和渲染结果，可选显示参考与渲染的深度、法线及细节误差图。"),
    ZH_HANT("並排顯示照片和算繪結果，可選顯示參考與算繪的深度、法線及細節誤差圖。"),
    KO("사진과 렌더링을 나란히 표시합니다. 참조 및 렌더링 깊이, 법선, 디테일 오차 맵도 선택하여 표시할 수 있습니다."),
    DE("Foto und Rendering nebeneinander anzeigen, optional mit Referenz- und gerenderter Tiefe, Normalen und Detailfehlerkarte."),
    FR("Affiche la photo et le rendu côte à côte, avec en option les profondeurs et normales de référence et rendues, ainsi que la carte d'erreur de détail."),
    ES("Muestra la foto y el render en paralelo, con profundidad y normales de referencia y renderizadas, y el mapa de error de detalle opcionales."),
    PT("Mostra a foto e a renderização lado a lado, com profundidade e normais de referência e renderizadas, e o mapa de erro de detalhe opcionais."),
    IT("Mostra foto e render affiancati, con profondità e normali di riferimento e del render e la mappa dell'errore di dettaglio opzionali."),
    NL("Toon foto en render naast elkaar, met optioneel referentie- en gerenderde diepte, normalen en de detailfoutkaart."),
    RU("Показ снимка и рендера рядом, с дополнительным показом эталонной глубины и нормалей, глубины и нормалей рендера и карты ошибки детализации."),
    TR("Fotoğraf ve görüntülemeyi yan yana gösterir; isteğe bağlı olarak referans ve görüntülenen derinlik, normaller ve ayrıntı hatası haritasını da gösterir."));

SS_MSG(compare_blend_help,
    EN("Overlays the selected photo/render, error, depth or normals pair. Adjust the blend to check alignment."),
    JA("選んだ写真と描画、誤差、深度、法線のペアを重ねます。混合を調整して位置合わせを確認できます。"),
    ZH_HANS("叠加选定的照片与渲染、误差、深度或法线对。调整混合比例以检查对齐。"),
    ZH_HANT("疊加選定的照片與算繪、誤差、深度或法線對。調整混合比例以檢查對齊。"),
    KO("선택한 사진/렌더링, 오차, 깊이 또는 법선 쌍을 겹칩니다. 혼합 비율로 정렬을 확인하세요."),
    DE("Überlagert das gewählte Foto/Render-, Fehler-, Tiefen- oder Normalenpaar. Mischung anpassen, um die Ausrichtung zu prüfen."),
    FR("Superpose la paire choisie : photo/rendu, erreur, profondeur ou normales. Ajuster le mélange pour vérifier l’alignement."),
    ES("Superpone el par elegido de foto/render, error, profundidad o normales. Ajusta la mezcla para comprobar la alineación."),
    PT("Sobrepõe o par escolhido de foto/render, erro, profundidade ou normais. Ajuste a mistura para verificar o alinhamento."),
    IT("Sovrappone la coppia scelta di foto/render, errore, profondità o normali. Regola la miscela per verificare l’allineamento."),
    NL("Legt het gekozen foto/render-, fout-, diepte- of normalenpaar over elkaar. Pas de menging aan om uitlijning te controleren."),
    RU("Накладывает выбранную пару фото/рендера, ошибки, глубины или нормалей. Меняйте смесь для проверки совмещения."),
    TR("Seçilen fotoğraf/işleme, hata, derinlik veya normal çiftini üst üste koyar. Hizalamayı denetlemek için karışımı ayarlayın."));

SS_MSG(compare_error_unavailable,
    EN("This run does not produce a detail error map."),
    JA("この学習では詳細の誤差マップを生成しません。"),
    ZH_HANS("本次训练不生成细节误差图。"), ZH_HANT("本次訓練不產生細節誤差圖。"),
    KO("이 학습에서는 디테일 오차 맵을 생성하지 않습니다."),
    DE("Dieser Trainingslauf erzeugt keine Detailfehlerkarte."),
    FR("Cet entraînement ne produit pas de carte d'erreur de détail."),
    ES("Este entrenamiento no genera un mapa de error de detalle."),
    PT("Este treinamento não produz um mapa de erro de detalhe."),
    IT("Questo addestramento non produce una mappa dell'errore di dettaglio."),
    NL("Deze training maakt geen detailfoutkaart."),
    RU("В этом запуске карта ошибки детализации не создаётся."),
    TR("Bu eğitim ayrıntı hatası haritası üretmez."));

SS_MSG(preview_mode_3d,
    EN("3D view"),       JA("3Dビュー"),      ZH_HANS("3D 视图"),  ZH_HANT("3D 檢視"),
    KO("3D 보기"),        DE("3D-Ansicht"),   FR("Vue 3D"),       ES("Vista 3D"),
    PT("Vista 3D"),      IT("Vista 3D"),     NL("3D-weergave"),  RU("3D-вид"),
    TR("3B görünüm"));

SS_MSG(preview_mode_images,
    EN("Images"),        JA("画像"),          ZH_HANS("图像"),     ZH_HANT("影像"),
    KO("이미지"),         DE("Bilder"),       FR("Images"),       ES("Imágenes"),
    PT("Imagens"),       IT("Immagini"),     NL("Beelden"),      RU("Изображения"),
    TR("Görüntüler"));

SS_MSG(preview_mode_help,
    EN("What the preview shows: the scene in 3D, or one training photograph "
       "beside the render of the same camera."),
    JA("プレビューに何を表示するかです。シーンを3Dで見るか、学習用の写真1枚と"
       "同じカメラのレンダリングを並べて見るかを選べます。"),
    ZH_HANS("预览显示什么：三维场景，或者一张训练照片与同一相机的渲染结果并排"
            "显示。"),
    ZH_HANT("預覽顯示什麼：三維場景，或者一張訓練照片與同一相機的算繪結果並排"
            "顯示。"),
    KO("미리보기에 무엇을 보여줄지 정합니다. 장면을 3D로 보거나, 학습용 사진 "
       "한 장과 같은 카메라의 렌더링을 나란히 봅니다."),
    DE("Was die Vorschau zeigt: die Szene in 3D oder ein Trainingsfoto neben "
       "dem Rendering derselben Kamera."),
    FR("Ce que montre l'aperçu : la scène en 3D, ou une photo d'entraînement à "
       "côté du rendu de la même caméra."),
    ES("Lo que muestra la vista previa: la escena en 3D, o una foto de "
       "entrenamiento junto al render de la misma cámara."),
    PT("O que a pré-visualização mostra: a cena em 3D, ou uma foto de "
       "treinamento ao lado da renderização da mesma câmera."),
    IT("Che cosa mostra l'anteprima: la scena in 3D, oppure una foto di "
       "addestramento accanto al render della stessa fotocamera."),
    NL("Wat de voorvertoning toont: de scène in 3D, of één trainingsfoto naast "
       "de render van dezelfde camera."),
    RU("Что показывает предпросмотр: сцену в 3D или один обучающий снимок рядом "
       "с рендером той же камеры."),
    TR("Önizlemenin ne göstereceği: sahne 3B olarak ya da bir eğitim fotoğrafı "
       "aynı kameranın görüntüsünün yanında."));

SS_MSG(compare_navigate_help,
    EN("Previous and next training image. The left and right arrow keys do the same; Page Up and Page Down move ten at a time, Home and End go to the first and the last."),
    JA("前後の学習画像に移動します。左右の矢印キーでも同じことができ、Page Up と Page Down は10枚ずつ、Home と End は最初と最後に移動します。"),
    ZH_HANS("切换到上一张或下一张训练图像。左右方向键作用相同；Page Up 和 Page Down 一次跳十张，Home 和 End 跳到第一张和最后一张。"),
    ZH_HANT("切換到上一張或下一張訓練影像。左右方向鍵作用相同；Page Up 和 Page Down 一次跳十張，Home 和 End 跳到第一張和最後一張。"),
    KO("이전·다음 학습 이미지로 이동합니다. 좌우 화살표 키도 같은 일을 하고, Page Up과 Page Down은 열 장씩, Home과 End는 처음과 끝으로 갑니다."),
    DE("Vorheriges und nächstes Trainingsbild. Die Pfeiltasten links und rechts tun dasselbe; Bild auf und Bild ab springen zehn weiter, Pos1 und Ende an den Anfang und ans Ende."),
    FR("Image d'entraînement précédente et suivante. Les flèches gauche et droite font la même chose ;\nPage précédente et Page suivante avancent de dix, Origine et Fin vont au début et à la fin."),
    ES("Imagen de entrenamiento anterior y siguiente. Las flechas izquierda y derecha hacen lo mismo; Re Pág y Av Pág saltan de diez en diez, Inicio y Fin van al principio y al final."),
    PT("Imagem de treinamento anterior e seguinte. As setas para a esquerda e para a direita fazem o mesmo; Page Up e Page Down saltam de dez em dez, Home e End vão ao início e ao fim."),
    IT("Immagine di addestramento precedente e successiva. Le frecce sinistra e destra fanno lo stesso; Pag su e Pag giù saltano di dieci, Inizio e Fine vanno al principio e alla fine."),
    NL("Vorig en volgend trainingsbeeld. De pijltjestoetsen links en rechts doen hetzelfde; Page Up en Page Down springen tien verder, Home en End naar het begin en het einde."),
    RU("Предыдущий и следующий обучающий снимок. Стрелки влево и вправо делают то же самое; Page Up и Page Down перемещают на десять, Home и End — в начало и в конец."),
    TR("Önceki ve sonraki eğitim görüntüsü. Sol ve sağ ok tuşları da aynı işi yapar; Page Up ve Page Down onar atlar, Home ve End başa ve sona gider."));

SS_MSG(compare_slider_help,
    EN("Which training image is shown. They are in file-name order."),
    JA("どの学習画像を表示するかです。並び順はファイル名順です。"),
    ZH_HANS("显示哪一张训练图像。顺序按文件名排列。"),
    ZH_HANT("顯示哪一張訓練影像。順序按檔名排列。"),
    KO("어떤 학습 이미지를 보여줄지 정합니다. 순서는 파일 이름순입니다."),
    DE("Welches Trainingsbild gezeigt wird. Die Reihenfolge ist die der "
       "Dateinamen."),
    FR("Quelle image d'entraînement est affichée. L'ordre est celui des noms de "
       "fichiers."),
    ES("Qué imagen de entrenamiento se muestra. El orden es el de los nombres "
       "de archivo."),
    PT("Qual imagem de treinamento é mostrada. A ordem é a dos nomes de "
       "arquivo."),
    IT("Quale immagine di addestramento è mostrata. L'ordine è quello dei nomi "
       "dei file."),
    NL("Welk trainingsbeeld wordt getoond. De volgorde is die van de "
       "bestandsnamen."),
    RU("Какой обучающий снимок показан. Порядок — по именам файлов."),
    TR("Hangi eğitim görüntüsünün gösterileceği. Sıralama dosya adına "
       "göredir."));

SS_MSG(compare_search_hint,
    EN("search file name"),
    JA("ファイル名で検索"),
    ZH_HANS("按文件名搜索"),
    ZH_HANT("以檔名搜尋"),
    KO("파일 이름 검색"),
    DE("Dateiname suchen"),
    FR("chercher un nom de fichier"),
    ES("buscar nombre de archivo"),
    PT("buscar nome de arquivo"),
    IT("cerca un nome di file"),
    NL("bestandsnaam zoeken"),
    RU("поиск по имени файла"),
    TR("dosya adı ara"));

SS_MSG(compare_pick_help,
    EN("Pick a training image by file name."),
    JA("ファイル名で学習画像を選びます。"),
    ZH_HANS("按文件名选择训练图像。"),
    ZH_HANT("以檔名選擇訓練影像。"),
    KO("파일 이름으로 학습 이미지를 고릅니다."),
    DE("Ein Trainingsbild über seinen Dateinamen wählen."),
    FR("Choisir une image d'entraînement par son nom de fichier."),
    ES("Elegir una imagen de entrenamiento por su nombre de archivo."),
    PT("Escolher uma imagem de treinamento pelo nome do arquivo."),
    IT("Scegliere un'immagine di addestramento dal nome del file."),
    NL("Een trainingsbeeld op bestandsnaam kiezen."),
    RU("Выбрать обучающий снимок по имени файла."),
    TR("Eğitim görüntüsünü dosya adından seçin."));

SS_MSG(compare_mask_dim,
    EN("Mask: dimmed"),  JA("マスク: 暗く表示"), ZH_HANS("遮罩：变暗"),
    ZH_HANT("遮罩：變暗"), KO("마스크: 어둡게"), DE("Maske: abgedunkelt"),
    FR("Masque : assombri"), ES("Máscara: atenuada"), PT("Máscara: escurecida"),
    IT("Maschera: scurita"), NL("Masker: gedimd"), RU("Маска: затемнить"),
    TR("Maske: karartılmış"));

SS_MSG(compare_mask_unmarked,
    EN("Mask: unmarked"), JA("マスク: 印なし"), ZH_HANS("遮罩：不标出"),
    ZH_HANT("遮罩：不標出"), KO("마스크: 표시 없음"), DE("Maske: unmarkiert"),
    FR("Masque : non marqué"), ES("Máscara: sin marcar"),
    PT("Máscara: sem marcação"), IT("Maschera: non segnata"),
    NL("Masker: niet gemarkeerd"), RU("Маска: без пометки"),
    TR("Maske: işaretsiz"));

SS_MSG(compare_mask_hide,
    EN("Mask: black"),   JA("マスク: 黒"), ZH_HANS("遮罩：黑色"),
    ZH_HANT("遮罩：黑色"), KO("마스크: 검게"),  DE("Maske: schwarz"),
    FR("Masque : noir"), ES("Máscara: en negro"), PT("Máscara: em preto"),
    IT("Maschera: in nero"), NL("Masker: zwart"), RU("Маска: чёрным"),
    TR("Maske: siyah"));

SS_MSG(compare_mask_help,
    EN("Display of excluded areas. Both panes mark them alike; training and the score ignore them."),
    JA("マスクで除外された部分の描き方です。学習はその部分をまったく見ないので、両側とも同じ印を付けます。下のスコアも残った部分だけで計算します。"),
    ZH_HANS("遮罩排除的部分怎么显示。训练完全不看这部分，所以两边用同样的方式标出；下面的分数也只统计剩下的部分。"),
    ZH_HANT("遮罩排除的部分怎麼顯示。訓練完全不看這部分，所以兩邊用同樣的方式標出；下面的分數也只統計剩下的部分。"),
    KO("마스크가 제외한 부분을 어떻게 그릴지 정합니다. 학습은 그 부분을 전혀 보지 않으므로 양쪽에 같은 표시를 하고, 아래 점수도 남은 부분만으로 계산합니다."),
    DE("Wie der von der Maske ausgeschlossene Teil gezeichnet wird. Das Training sieht ihn gar nicht, also markieren ihn beide Seiten gleich -- und der Wert darunter zählt nur den Rest."),
    FR("Affichage des zones exclues. Les deux vues les marquent de même ; entraînement et score les ignorent."),
    ES("Cómo se dibuja la parte que la máscara excluye. El entrenamiento no la ve en absoluto, así que ambos lados la marcan igual, y la puntuación de abajo solo cuenta el resto."),
    PT("Como é desenhada a parte que a máscara exclui. O treinamento não a vê, então os dois lados a marcam do mesmo jeito, e a pontuação abaixo conta apenas o resto."),
    IT("Aspetto delle aree escluse. Entrambe le viste le segnano allo stesso modo; addestramento e punteggio le ignorano."),
    NL("Weergave van uitgesloten gebieden. Beide beelden markeren ze gelijk; training en score negeren ze."),
    RU("Как рисуется часть, исключённая маской. Обучение её вовсе не видит, поэтому обе стороны помечают её одинаково, а оценка ниже считает только остальное."),
    TR("Maskenin dışladığı bölümün nasıl çizileceği. Eğitim orayı hiç görmez, bu yüzden iki taraf da aynı biçimde işaretler ve aşağıdaki puan yalnızca kalanı sayar."));

SS_MSG(compare_color_match,
    EN("Colour match"), JA("色を合わせる"),   ZH_HANS("匹配色彩"), ZH_HANT("比對色彩"),
    KO("색 맞추기"),     DE("Farbabgleich"), FR("Accord des couleurs"),
    ES("Ajuste de color"), PT("Ajuste de cor"), IT("Adattamento del colore"),
    NL("Kleuraanpassing"), RU("Подгонка цвета"), TR("Renk eşleme"));

SS_MSG(compare_color_match_help,
    EN("Apply this run's per-image exposure and colour correction to the "
       "render, as the loss does. Turn it off to see the model's own colours."),
    JA("この実行で使っている画像ごとの露出と色の補正を、損失の計算と同じように"
       "レンダリングにも適用します。オフにするとモデル本来の色が見えます。"),
    ZH_HANS("把本次训练中每张图像的曝光与色彩校正也应用到渲染上，和计算损失时"
            "一样。关掉就能看到模型本身的颜色。"),
    ZH_HANT("把本次訓練中每張影像的曝光與色彩校正也套用到算繪上，和計算損失時"
            "一樣。關掉就能看到模型本身的顏色。"),
    KO("이번 학습이 이미지마다 쓰는 노출·색 보정을 손실 계산과 똑같이 "
       "렌더링에도 적용합니다. 끄면 모델 자체의 색이 보입니다."),
    DE("Die bildweise Belichtungs- und Farbkorrektur dieses Laufs auf das "
       "Rendering anwenden, so wie der Verlust es tut. Ausgeschaltet zeigt es "
       "die Farben des Modells selbst."),
    FR("Appliquer au rendu la correction d'exposition et de couleur propre à "
       "chaque image, comme le fait la perte. Désactivé, on voit les couleurs "
       "du modèle lui-même."),
    ES("Aplicar al render la corrección de exposición y color por imagen de "
       "esta ejecución, como hace la pérdida. Desactivado, se ven los colores "
       "del propio modelo."),
    PT("Aplicar à renderização a correção de exposição e cor por imagem desta "
       "execução, como a perda faz. Desligado, veem-se as cores do próprio "
       "modelo."),
    IT("Applicare al render la correzione di esposizione e colore per immagine "
       "di questa esecuzione, come fa la perdita. Disattivato, si vedono i "
       "colori del modello stesso."),
    NL("De belichtings- en kleurcorrectie per beeld van deze run op de render "
       "toepassen, zoals het verlies dat doet. Uit toont de kleuren van het "
       "model zelf."),
    RU("Применить к рендеру покадровую коррекцию экспозиции и цвета этого "
       "запуска — так же, как это делает функция потерь. Выключено — видны "
       "собственные цвета модели."),
    TR("Bu çalışmanın görüntü başına pozlama ve renk düzeltmesini, kayıp "
       "işlevinin yaptığı gibi görüntülemeye de uygula. Kapalıyken modelin "
       "kendi renkleri görünür."));

SS_MSG(compare_source_gt,
    EN("Original file"), JA("元のファイル"),   ZH_HANS("原始文件"), ZH_HANT("原始檔案"),
    KO("원본 파일"),      DE("Originaldatei"), FR("Fichier d'origine"),
    ES("Archivo original"), PT("Arquivo original"), IT("File originale"),
    NL("Oorspronkelijk bestand"), RU("Исходный файл"), TR("Özgün dosya"));

SS_MSG(compare_source_gt_help,
    EN("Show the photograph at its own resolution instead of the downscaled "
       "copy training feeds the loss. Zoom in to see what the downscale costs."),
    JA("学習が損失に渡している縮小後のコピーではなく、写真そのものの解像度で"
       "表示します。拡大すると、縮小で失われた細部がわかります。"),
    ZH_HANS("按照片本身的分辨率显示，而不是训练送进损失的缩小副本。放大就能看出"
            "缩小损失了多少细节。"),
    ZH_HANT("按照片本身的解析度顯示，而不是訓練送進損失的縮小副本。放大就能看出"
            "縮小損失了多少細節。"),
    KO("학습이 손실에 넣는 축소본 대신 사진 본래 해상도로 보여줍니다. 확대하면 "
       "축소로 잃은 디테일이 얼마나 되는지 알 수 있습니다."),
    DE("Das Foto in seiner eigenen Auflösung zeigen statt der verkleinerten "
       "Kopie, die das Training dem Verlust gibt. Hineinzoomen zeigt, was die "
       "Verkleinerung kostet."),
    FR("Montrer la photo dans sa propre résolution plutôt que la copie réduite "
       "que l'entraînement donne à la perte. En zoomant, on voit ce que la "
       "réduction coûte."),
    ES("Mostrar la foto en su propia resolución en vez de la copia reducida que "
       "el entrenamiento pasa a la pérdida. Al ampliar se ve lo que cuesta la "
       "reducción."),
    PT("Mostrar a foto na sua própria resolução em vez da cópia reduzida que o "
       "treinamento passa à perda. Ao ampliar, vê-se o que a redução custa."),
    IT("Mostrare la foto alla sua risoluzione invece della copia ridotta che "
       "l'addestramento passa alla perdita. Ingrandendo si vede quanto costa la "
       "riduzione."),
    NL("De foto op zijn eigen resolutie tonen in plaats van de verkleinde kopie "
       "die het trainen aan het verlies geeft. Inzoomen laat zien wat het "
       "verkleinen kost."),
    RU("Показать снимок в его собственном разрешении, а не уменьшённую копию, "
       "которую обучение передаёт функции потерь. При увеличении видно, во что "
       "обходится уменьшение."),
    TR("Fotoğrafı, eğitimin kayba verdiği küçültülmüş kopya yerine kendi "
       "çözünürlüğünde göster. Yakınlaştırınca küçültmenin neye mal olduğu "
       "görünür."));

SS_MSG(compare_source_gt_split,
    EN("Not available for a split image: the views the loss sees do not line up "
       "with any single source frame."),
    JA("分割された画像では使えません。損失が見ている各ビューは、元の1枚の画像と"
       "そのまま重なりません。"),
    ZH_HANS("拆分后的图像不能用：损失看到的各个视图和原来的单张图像对不上。"),
    ZH_HANT("拆分後的影像不能用：損失看到的各個檢視和原本的單張影像對不上。"),
    KO("분할된 이미지에서는 쓸 수 없습니다. 손실이 보는 각 뷰는 원본 한 장과 "
       "그대로 겹치지 않습니다."),
    DE("Bei einem aufgeteilten Bild nicht verfügbar: die Ansichten, die der "
       "Verlust sieht, decken sich mit keiner einzelnen Quellaufnahme."),
    FR("Indisponible pour une image découpée : les vues que voit la perte ne "
       "coïncident avec aucune image source unique."),
    ES("No disponible para una imagen dividida: las vistas que ve la pérdida no "
       "coinciden con ninguna imagen de origen."),
    PT("Indisponível para uma imagem dividida: as vistas que a perda vê não "
       "coincidem com nenhuma imagem de origem."),
    IT("Non disponibile per un'immagine divisa: le viste che la perdita vede "
       "non coincidono con nessuna immagine sorgente."),
    NL("Niet beschikbaar bij een opgesplitst beeld: de weergaven die het "
       "verlies ziet vallen met geen enkel bronbeeld samen."),
    RU("Недоступно для разрезанного снимка: виды, которые видит функция потерь, "
       "не совпадают ни с одним исходным кадром."),
    TR("Bölünmüş bir görüntüde kullanılamaz: kaybın gördüğü görünümler tek bir "
       "kaynak kareyle örtüşmez."));

SS_MSG(compare_calib_warp,
    EN("Calibration warp"), JA("キャリブレーションワープ"),
    ZH_HANS("标定形变"), ZH_HANT("校準形變"),
    KO("캘리브레이션 워프"), DE("Kalibrierverzerrung"),
    FR("Déformation d'étalonnage"), ES("Deformación de calibración"),
    PT("Deformação de calibração"), IT("Deformazione di calibrazione"),
    NL("Kalibratievervorming"), RU("Деформация калибровки"),
    TR("Kalibrasyon bükmesi"));

SS_MSG(compare_calib_warp_help,
    EN("Show the reference through this image's calibration warp, as the loss "
       "reads it. The mask, depth and normals follow."),
    JA("損失が読むのと同じく、この画像のキャリブレーションワープを通した参照を"
       "表示します。マスク・深度・法線も同様です。"),
    ZH_HANS("像损失读取时那样，显示经过这张图像标定形变的参考图。掩码、深度和法线"
            "也一样。"),
    ZH_HANT("像損失讀取時那樣，顯示經過這張影像校準形變的參考圖。遮罩、深度和法線"
            "也一樣。"),
    KO("손실이 읽는 그대로, 이 이미지의 캘리브레이션 워프를 거친 참조를 "
       "보여줍니다. 마스크, 깊이, 법선도 같습니다."),
    DE("Die Referenz durch die Kalibrierverzerrung dieses Bildes zeigen, so wie "
       "der Verlust sie liest. Maske, Tiefe und Normalen ebenso."),
    FR("Montrer la référence à travers la déformation d'étalonnage de cette "
       "image, telle que la perte la lit. Masque, profondeur et normales aussi."),
    ES("Mostrar la referencia a través de la deformación de calibración de esta "
       "imagen, como la lee la pérdida. También máscara, profundidad y normales."),
    PT("Mostrar a referência através da deformação de calibração desta imagem, "
       "como a perda a lê. Também máscara, profundidade e normais."),
    IT("Mostrare il riferimento attraverso la deformazione di calibrazione di "
       "questa immagine, come lo legge la perdita. Anche maschera, profondità e "
       "normali."),
    NL("De referentie tonen door de kalibratievervorming van dit beeld, zoals "
       "het verlies die leest. Masker, diepte en normalen ook."),
    RU("Показать эталон через деформацию калибровки этого снимка — так, как его "
       "читает функция потерь. Маска, глубина и нормали тоже."),
    TR("Referansı, kaybın okuduğu gibi bu görüntünün kalibrasyon bükmesinden "
       "geçirerek göster. Maske, derinlik ve normaller de öyle."));

SS_MSG(compare_smooth,
    EN("Smooth"),        JA("なめらか"),      ZH_HANS("平滑"),     ZH_HANT("平滑"),
    KO("부드럽게"),       DE("Weich"),        FR("Lissé"),        ES("Suave"),
    PT("Suave"),         IT("Morbido"),      NL("Vloeiend"),     RU("Сглаживание"),
    TR("Yumuşak"));

SS_MSG(compare_smooth_help,
    EN("Interpolate between pixels when zoomed in. Switch it off to see the "
       "individual pixels."),
    JA("拡大したときにピクセルの間を補間します。オフにすると1ピクセルずつ"
       "そのまま見えます。"),
    ZH_HANS("放大时在像素之间平滑过渡。关掉就能看到一个个像素。"),
    ZH_HANT("放大時在像素之間平滑過渡。關掉就能看到一個個像素。"),
    KO("확대했을 때 픽셀 사이를 보간합니다. 끄면 픽셀 하나하나가 그대로 "
       "보입니다."),
    DE("Beim Hineinzoomen zwischen den Pixeln interpolieren. Ausgeschaltet "
       "zeigt es die einzelnen Pixel."),
    FR("Interpoler entre les pixels lors du zoom. Désactivé, les pixels "
       "apparaissent un à un."),
    ES("Interpolar entre píxeles al ampliar. Desactivado, se ven los píxeles "
       "uno a uno."),
    PT("Interpolar entre os pixels ao ampliar. Desligado, veem-se os pixels um "
       "a um."),
    IT("Interpolare tra i pixel quando si ingrandisce. Disattivato, i pixel si "
       "vedono uno per uno."),
    NL("Bij inzoomen tussen de pixels interpoleren. Uit toont de afzonderlijke "
       "pixels."),
    RU("Интерполировать между пикселями при увеличении. Выключено — видны "
       "отдельные пиксели."),
    TR("Yakınlaştırırken pikseller arasında ara değer hesapla. Kapalıyken "
       "pikseller tek tek görünür."));

SS_MSG(compare_reset_zoom,
    EN("Fit"),           JA("全体表示"),      ZH_HANS("适应窗口"), ZH_HANT("符合視窗"),
    KO("전체 보기"),      DE("Einpassen"),    FR("Ajuster"),      ES("Ajustar"),
    PT("Ajustar"),       IT("Adatta"),       NL("Passend"),      RU("Вписать"),
    TR("Sığdır"));

SS_MSG(compare_zoom_help,
    EN("The scroll wheel zooms where the cursor is and dragging moves the "
       "image. Both sides follow one another."),
    JA("ホイールでカーソルの位置を中心に拡大縮小し、ドラッグで画像を動かします。"
       "左右の表示は連動します。"),
    ZH_HANS("滚轮以光标处为中心缩放，拖动可平移图像。两边始终保持一致。"),
    ZH_HANT("滾輪以游標處為中心縮放，拖動可平移影像。兩邊始終保持一致。"),
    KO("휠은 커서 위치를 중심으로 확대·축소하고, 끌면 이미지가 움직입니다. "
       "양쪽은 서로 따라갑니다."),
    DE("Das Mausrad zoomt dort, wo der Zeiger steht, Ziehen verschiebt das "
       "Bild. Beide Seiten folgen einander."),
    FR("La molette zoome à l'endroit du curseur et le glissement déplace "
       "l'image. Les deux côtés se suivent."),
    ES("La rueda amplía donde está el cursor y arrastrar mueve la imagen. Los "
       "dos lados se siguen."),
    PT("A roda amplia onde está o cursor e arrastar move a imagem. Os dois "
       "lados se acompanham."),
    IT("La rotella ingrandisce dove si trova il cursore e il trascinamento "
       "sposta l'immagine. I due lati si seguono."),
    NL("Het scrollwiel zoomt waar de aanwijzer staat en slepen verschuift het "
       "beeld. Beide kanten volgen elkaar."),
    RU("Колесо увеличивает там, где стоит курсор, перетаскивание сдвигает "
       "изображение. Обе стороны следуют друг за другом."),
    TR("Tekerlek imlecin bulunduğu yerde yakınlaştırır, sürüklemek görüntüyü "
       "kaydırır. İki taraf birbirini izler."));

SS_MSG(compare_pane_gt,
    EN("Photograph"),    JA("写真"),          ZH_HANS("照片"),     ZH_HANT("照片"),
    KO("사진"),           DE("Foto"),         FR("Photo"),        ES("Foto"),
    PT("Foto"),          IT("Foto"),         NL("Foto"),         RU("Снимок"),
    TR("Fotoğraf"));

SS_MSG(compare_pane_render,
    EN("Render"),        JA("レンダリング"),   ZH_HANS("渲染"),     ZH_HANT("算繪"),
    KO("렌더링"),         DE("Rendering"),    FR("Rendu"),        ES("Render"),
    PT("Renderização"),  IT("Render"),       NL("Render"),       RU("Рендер"),
    TR("Görüntüleme"));

SS_MSG(compare_pane_gt_depth,
    EN("Reference depth"),
    JA("参照の深度"),      ZH_HANS("参考深度"),  ZH_HANT("參考深度"),
    KO("참조 깊이"),       DE("Referenztiefe"), FR("Profondeur de référence"),
    ES("Profundidad de referencia"), PT("Profundidade de referência"),
    IT("Profondità di riferimento"), NL("Referentiediepte"),
    RU("Эталонная глубина"), TR("Referans derinlik"));

SS_MSG(compare_pane_render_depth,
    EN("Rendered depth"),
    JA("レンダリングの深度"), ZH_HANS("渲染深度"),  ZH_HANT("算繪深度"),
    KO("렌더링 깊이"),      DE("Gerenderte Tiefe"), FR("Profondeur rendue"),
    ES("Profundidad renderizada"), PT("Profundidade renderizada"),
    IT("Profondità del render"), NL("Gerenderde diepte"),
    RU("Глубина рендера"), TR("Görüntülenen derinlik"));

SS_MSG(compare_pane_gt_normal,
    EN("Reference normals"),
    JA("参照の法線"),      ZH_HANS("参考法线"),  ZH_HANT("參考法線"),
    KO("참조 법선"),       DE("Referenznormalen"), FR("Normales de référence"),
    ES("Normales de referencia"), PT("Normais de referência"),
    IT("Normali di riferimento"), NL("Referentienormalen"),
    RU("Эталонные нормали"), TR("Referans normaller"));

SS_MSG(compare_pane_render_normal,
    EN("Rendered normals"),
    JA("レンダリングの法線"), ZH_HANS("渲染法线"),  ZH_HANT("算繪法線"),
    KO("렌더링 법선"),      DE("Gerenderte Normalen"), FR("Normales rendues"),
    ES("Normales renderizadas"), PT("Normais renderizadas"),
    IT("Normali del render"), NL("Gerenderde normalen"),
    RU("Нормали рендера"), TR("Görüntülenen normaller"));

SS_MSG(compare_pane_error,
    EN("Detail error"),
    JA("詳細の誤差"),      ZH_HANS("细节误差"),  ZH_HANT("細節誤差"),
    KO("디테일 오차"),      DE("Detailfehler"), FR("Erreur de détail"),
    ES("Error de detalle"), PT("Erro de detalhe"),
    IT("Errore di dettaglio"), NL("Detailfout"),
    RU("Ошибка детализации"), TR("Ayrıntı hatası"));

SS_MSG(compare_show_error,
    EN("Error map"),     JA("誤差マップ"),     ZH_HANS("误差图"),   ZH_HANT("誤差圖"),
    KO("오차 맵"),        DE("Fehlerkarte"),  FR("Carte d'erreur"), ES("Mapa de error"),
    PT("Mapa de erro"),  IT("Mappa dell'errore"), NL("Foutkaart"),
    RU("Карта ошибки"),  TR("Hata haritası"));

SS_MSG(compare_show_error_help,
    EN("Shows the processed detail error used to place new splats. Warm colors mark areas needing more detail."),
    JA("新しいスプラットの配置に使う、処理済みの詳細誤差を表示します。暖色は詳細が不足する領域です。"),
    ZH_HANS("显示用于放置新高斯的处理后细节误差。暖色标出需要更多细节的区域。"),
    ZH_HANT("顯示用於放置新高斯的處理後細節誤差。暖色標出需要更多細節的區域。"),
    KO("새 스플랫 배치에 쓰이는 처리된 세부 오차를 표시합니다. 따뜻한 색은 세부가 더 필요한 영역입니다."),
    DE("Zeigt den verarbeiteten Detailfehler für die Platzierung neuer Splats. Warme Farben markieren Bereiche mit Detailbedarf."),
    FR("Affiche l’erreur de détail traitée servant à placer les nouveaux splats. Les couleurs chaudes signalent les zones à détailler."),
    ES("Muestra el error de detalle procesado que guía los nuevos splats. Los colores cálidos indican zonas que necesitan más detalle."),
    PT("Mostra o erro de detalhe processado que guia novos splats. As cores quentes indicam áreas que precisam de mais detalhe."),
    IT("Mostra l’errore di dettaglio elaborato che guida i nuovi splat. I colori caldi indicano aree che richiedono più dettaglio."),
    NL("Toont de verwerkte detailfout die nieuwe splats stuurt. Warme kleuren markeren gebieden die meer detail nodig hebben."),
    RU("Показывает обработанную ошибку детализации для размещения новых сплатов. Тёплые цвета отмечают области, которым нужны детали."),
    TR("Yeni splat yerleşimini yönlendiren işlenmiş ayrıntı hatasını gösterir. Sıcak renkler daha fazla ayrıntı gereken alanları belirtir."));

SS_MSG(compare_error_stats,
    EN("Error: mean {0}, max {1}"),
    JA("誤差: 平均 {0}、最大 {1}"),
    ZH_HANS("误差：平均 {0}，最大 {1}"),
    ZH_HANT("誤差：平均 {0}，最大 {1}"),
    KO("오차: 평균 {0}, 최대 {1}"),
    DE("Fehler: Mittel {0}, Max. {1}"),
    FR("Erreur : moyenne {0}, max {1}"),
    ES("Error: media {0}, máx. {1}"),
    PT("Erro: média {0}, máx. {1}"),
    IT("Errore: media {0}, max {1}"),
    NL("Fout: gemiddeld {0}, max {1}"),
    RU("Ошибка: среднее {0}, максимум {1}"),
    TR("Hata: ortalama {0}, en çok {1}"));

SS_MSG(compare_show_depth,
    EN("Depth"),         JA("深度"),          ZH_HANS("深度"),     ZH_HANT("深度"),
    KO("깊이"),           DE("Tiefe"),        FR("Profondeur"),   ES("Profundidad"),
    PT("Profundidade"),  IT("Profondità"),   NL("Diepte"),       RU("Глубина"),
    TR("Derinlik"));

SS_MSG(compare_show_depth_help,
    EN("Compares reference and rendered depth. Each uses its own color range: compare shape, not absolute depth."),
    JA("参照深度と描画深度を比較します。色の範囲はそれぞれ異なるため、絶対深度ではなく形を比較してください。"),
    ZH_HANS("对比参考深度与渲染深度。各自使用独立色阶：请比较形状，而非绝对深度。"),
    ZH_HANT("對比參考深度與算繪深度。各自使用獨立色階：請比較形狀，而非絕對深度。"),
    KO("참조 깊이와 렌더링 깊이를 비교합니다. 색 범위가 각각 다르므로 절대 깊이가 아닌 형태를 비교하세요."),
    DE("Vergleicht Referenz- und gerenderte Tiefe. Jede hat ihren eigenen Farbbereich: Form vergleichen, keine absoluten Tiefen."),
    FR("Compare la profondeur de référence au rendu. Chacune a sa propre échelle de couleurs : comparer la forme, pas la profondeur absolue."),
    ES("Compara la profundidad de referencia y renderizada. Cada una usa su propia escala de color: compara formas, no profundidades absolutas."),
    PT("Compara a profundidade de referência e renderizada. Cada uma usa a sua escala de cores: compare formas, não profundidades absolutas."),
    IT("Confronta profondità di riferimento e renderizzata. Ognuna usa la propria scala cromatica: confronta la forma, non la profondità assoluta."),
    NL("Vergelijkt referentie- en gerenderde diepte. Elk heeft een eigen kleurbereik: vergelijk de vorm, niet de absolute diepte."),
    RU("Сравнивает эталонную глубину с рендером. Цветовые шкалы независимы: сравнивайте форму, а не абсолютную глубину."),
    TR("Referans ve işlenen derinliği karşılaştırır. Renk aralıkları ayrıdır: mutlak derinliği değil, şekli karşılaştırın."));

SS_MSG(compare_show_normal,
    EN("Normals"),        JA("法線"),          ZH_HANS("法线"),     ZH_HANT("法線"),
    KO("법선"),            DE("Normalen"),     FR("Normales"),     ES("Normales"),
    PT("Normais"),        IT("Normali"),      NL("Normalen"),     RU("Нормали"),
    TR("Normaller"));

SS_MSG(compare_show_normal_help,
    EN("Compares reference normals with normals derived from rendered depth, as used by the normal loss."),
    JA("参照法線と、描画深度から求めた法線を比較します。法線損失と同じ比較です。"),
    ZH_HANS("对比参考法线与渲染深度推导的法线，与法线损失使用的比较一致。"),
    ZH_HANT("對比參考法線與算繪深度推導的法線，與法線損失使用的比較一致。"),
    KO("참조 법선과 렌더링 깊이에서 구한 법선을 비교합니다. 법선 손실에 쓰이는 비교입니다."),
    DE("Vergleicht Referenznormalen mit den aus gerenderter Tiefe abgeleiteten Normalen, wie beim Normalenverlust."),
    FR("Compare les normales de référence à celles déduites de la profondeur rendue, comme pour la perte sur les normales."),
    ES("Compara las normales de referencia con las derivadas de la profundidad renderizada, igual que la pérdida de normales."),
    PT("Compara as normais de referência com as derivadas da profundidade renderizada, como na perda de normais."),
    IT("Confronta le normali di riferimento con quelle derivate dalla profondità renderizzata, come nella perdita sulle normali."),
    NL("Vergelijkt referentienormalen met normalen uit de gerenderde diepte, zoals bij het normaalverlies."),
    RU("Сравнивает эталонные нормали с нормалями из отрисованной глубины, как при расчёте потерь по нормалям."),
    TR("Referans normallerini, normal kaybında olduğu gibi, işlenen derinlikten türetilen normallerle karşılaştırır."));

SS_MSG(compare_waiting,
    EN("Start training to put a photograph next to the render of the same "
       "camera."),
    JA("学習を開始すると、写真と同じカメラのレンダリングを並べて表示できます。"),
    ZH_HANS("开始训练后，就能把照片和同一相机的渲染结果并排看。"),
    ZH_HANT("開始訓練後，就能把照片和同一相機的算繪結果並排看。"),
    KO("학습을 시작하면 사진과 같은 카메라의 렌더링을 나란히 볼 수 있습니다."),
    DE("Starten Sie das Training, um ein Foto neben das Rendering derselben "
       "Kamera zu stellen."),
    FR("Lancez l'entraînement pour placer une photo à côté du rendu de la même "
       "caméra."),
    ES("Inicie el entrenamiento para poner una foto junto al render de la misma "
       "cámara."),
    PT("Inicie o treinamento para pôr uma foto ao lado da renderização da mesma "
       "câmera."),
    IT("Avvii l'addestramento per mettere una foto accanto al render della "
       "stessa fotocamera."),
    NL("Start het trainen om een foto naast de render van dezelfde camera te "
       "zetten."),
    RU("Запустите обучение, чтобы поставить снимок рядом с рендером той же "
       "камеры."),
    TR("Bir fotoğrafı aynı kameranın görüntüsünün yanına koymak için eğitimi "
       "başlatın."));

SS_MSG(compare_trained_size,
    EN("Trained at: {0} x {1}"),
    JA("学習解像度: {0} x {1}"),
    ZH_HANS("训练分辨率：{0} x {1}"),
    ZH_HANT("訓練解析度：{0} x {1}"),
    KO("학습 해상도: {0} x {1}"),
    DE("Trainingsgröße: {0} x {1}"),
    FR("Taille d'entraînement : {0} x {1}"),
    ES("Tamaño de entrenamiento: {0} x {1}"),
    PT("Tamanho de treinamento: {0} x {1}"),
    IT("Dimensione di addestramento: {0} x {1}"),
    NL("Trainingsformaat: {0} x {1}"),
    RU("Размер при обучении: {0} x {1}"),
    TR("Eğitim boyutu: {0} x {1}"));

SS_MSG(compare_trained_size_help,
    EN("The size the loss actually sees. It is smaller than the file when the "
       "dataset is being downscaled."),
    JA("損失が実際に見ているサイズです。データセットを縮小して学習している場合は、"
       "ファイルより小さくなります。"),
    ZH_HANS("损失实际看到的尺寸。如果数据集缩小后再训练，它就比文件本身小。"),
    ZH_HANT("損失實際看到的尺寸。如果資料集縮小後再訓練，它就比檔案本身小。"),
    KO("손실이 실제로 보는 크기입니다. 데이터셋을 축소해 학습하면 파일보다 "
       "작습니다."),
    DE("Die Größe, die der Verlust tatsächlich sieht. Wird der Datensatz "
       "verkleinert, ist sie kleiner als die Datei."),
    FR("La taille que la perte voit réellement. Elle est plus petite que le "
       "fichier lorsque le jeu de données est réduit."),
    ES("El tamaño que la pérdida ve realmente. Es menor que el archivo cuando "
       "el conjunto de datos se reduce."),
    PT("O tamanho que a perda realmente vê. É menor que o arquivo quando o "
       "conjunto de dados é reduzido."),
    IT("La dimensione che la perdita vede davvero. È più piccola del file "
       "quando il set di dati viene ridotto."),
    NL("Het formaat dat het verlies werkelijk ziet. Het is kleiner dan het "
       "bestand wanneer de dataset verkleind wordt."),
    RU("Размер, который на самом деле видит функция потерь. Он меньше файла, "
       "если набор данных уменьшают."),
    TR("Kaybın gerçekte gördüğü boyut. Veri kümesi küçültülüyorsa dosyadan "
       "küçüktür."));

SS_MSG(compare_source_size,
    EN("File: {0} x {1}"),
    JA("ファイル: {0} x {1}"),
    ZH_HANS("文件：{0} x {1}"),
    ZH_HANT("檔案：{0} x {1}"),
    KO("파일: {0} x {1}"),
    DE("Datei: {0} x {1}"),
    FR("Fichier : {0} x {1}"),
    ES("Archivo: {0} x {1}"),
    PT("Arquivo: {0} x {1}"),
    IT("File: {0} x {1}"),
    NL("Bestand: {0} x {1}"),
    RU("Файл: {0} x {1}"),
    TR("Dosya: {0} x {1}"));

SS_MSG(compare_faces,
    EN("Views: {0}"),
    JA("ビュー数: {0}"),
    ZH_HANS("视图数：{0}"),
    ZH_HANT("檢視數：{0}"),
    KO("뷰 수: {0}"),
    DE("Ansichten: {0}"),
    FR("Vues : {0}"),
    ES("Vistas: {0}"),
    PT("Vistas: {0}"),
    IT("Viste: {0}"),
    NL("Weergaven: {0}"),
    RU("Видов: {0}"),
    TR("Görünüm: {0}"));

SS_MSG(compare_faces_help,
    EN("This lens is split into several flat views before training, and both "
       "panes show all of them in one grid."),
    JA("このレンズは学習前に複数の平面ビューに分割されます。左右のパネルは"
       "それらをまとめて1つの格子で表示します。"),
    ZH_HANS("这种镜头在训练前会被拆成若干个平面视图，两边面板都把它们排在同一个"
            "网格里显示。"),
    ZH_HANT("這種鏡頭在訓練前會被拆成數個平面檢視，兩邊面板都把它們排在同一個"
            "格線裡顯示。"),
    KO("이 렌즈는 학습 전에 여러 개의 평면 뷰로 나누어지며, 양쪽 패널은 그것들을 "
       "한 격자에 모아 보여줍니다."),
    DE("Dieses Objektiv wird vor dem Training in mehrere flache Ansichten "
       "zerlegt; beide Felder zeigen sie zusammen in einem Raster."),
    FR("Cet objectif est découpé en plusieurs vues planes avant l'entraînement, "
       "et les deux panneaux les montrent toutes dans une grille."),
    ES("Este objetivo se divide en varias vistas planas antes de entrenar, y "
       "ambos paneles las muestran todas en una cuadrícula."),
    PT("Esta lente é dividida em várias vistas planas antes do treinamento, e "
       "os dois painéis mostram todas elas numa grade."),
    IT("Questo obiettivo viene diviso in più viste piane prima "
       "dell'addestramento, ed entrambi i pannelli le mostrano tutte in una "
       "griglia."),
    NL("Deze lens wordt voor het trainen in meerdere vlakke weergaven "
       "gesplitst; beide panelen tonen ze samen in één raster."),
    RU("Этот объектив перед обучением разрезается на несколько плоских видов, и "
       "обе панели показывают их все в одной сетке."),
    TR("Bu mercek eğitimden önce birkaç düz görünüme bölünür; iki panel de "
       "hepsini tek bir ızgarada gösterir."));

SS_MSG(compare_psnr,
    EN("PSNR: {0} dB"),
    JA("PSNR: {0} dB"),
    ZH_HANS("PSNR：{0} dB"),
    ZH_HANT("PSNR：{0} dB"),
    KO("PSNR: {0} dB"),
    DE("PSNR: {0} dB"),
    FR("PSNR : {0} dB"),
    ES("PSNR: {0} dB"),
    PT("PSNR: {0} dB"),
    IT("PSNR: {0} dB"),
    NL("PSNR: {0} dB"),
    RU("PSNR: {0} дБ"),
    TR("PSNR: {0} dB"));

SS_MSG(compare_psnr_none,
    EN("PSNR: not available"),
    JA("PSNR: なし"),
    ZH_HANS("PSNR：无"),
    ZH_HANT("PSNR：無"),
    KO("PSNR: 없음"),
    DE("PSNR: nicht verfügbar"),
    FR("PSNR : indisponible"),
    ES("PSNR: no disponible"),
    PT("PSNR: indisponível"),
    IT("PSNR: non disponibile"),
    NL("PSNR: niet beschikbaar"),
    RU("PSNR: нет"),
    TR("PSNR: yok"));

SS_MSG(compare_psnr_help,
    EN("How close the render is to the photograph, over the pixels the loss "
       "counts. Higher is better."),
    JA("レンダリングが写真にどれだけ近いかを、損失が数える画素について示します。"
       "大きいほど良い値です。"),
    ZH_HANS("在损失统计的像素上，渲染与照片有多接近。数值越大越好。"),
    ZH_HANT("在損失統計的像素上，算繪與照片有多接近。數值越大越好。"),
    KO("손실이 세는 픽셀에 대해 렌더링이 사진에 얼마나 가까운지를 나타냅니다. "
       "값이 클수록 좋습니다."),
    DE("Wie nah das Rendering dem Foto kommt, über die Pixel, die der Verlust "
       "zählt. Höher ist besser."),
    FR("À quel point le rendu approche la photo, sur les pixels que la perte "
       "compte. Plus la valeur est haute, mieux c'est."),
    ES("Cuánto se acerca el render a la foto, sobre los píxeles que cuenta la "
       "pérdida. Cuanto más alto, mejor."),
    PT("O quanto a renderização se aproxima da foto, sobre os pixels que a "
       "perda conta. Quanto mais alto, melhor."),
    IT("Quanto il render si avvicina alla foto, sui pixel che la perdita conta. "
       "Più alto è, meglio è."),
    NL("Hoe dicht de render bij de foto komt, over de pixels die het verlies "
       "telt. Hoger is beter."),
    RU("Насколько рендер близок к снимку по тем пикселям, которые считает "
       "функция потерь. Чем выше, тем лучше."),
    TR("Kaybın saydığı pikseller üzerinde görüntülemenin fotoğrafa ne kadar "
       "yaklaştığı. Yüksek olması iyidir."));

SS_MSG(compare_at_step,
    EN("at step {0}"),
    JA("ステップ {0} 時点"),
    ZH_HANS("第 {0} 步时"),
    ZH_HANT("第 {0} 步時"),
    KO("{0} 단계 시점"),
    DE("bei Schritt {0}"),
    FR("à l'étape {0}"),
    ES("en el paso {0}"),
    PT("no passo {0}"),
    IT("al passo {0}"),
    NL("bij stap {0}"),
    RU("на шаге {0}"),
    TR("{0}. adımda"));

// ===========================================================================
// Crash report
// ===========================================================================
// The path follows on its own line rather than interpolated into the
// sentence: the handler that shows this may not allocate.

SS_MSG(crash_report_saved,
    EN("Spirula Studio stopped unexpectedly. A report was saved to the file "
       "below; please attach it to a bug report."),
    JA("スピルラ・スタジオが予期せず終了しました。下のファイルにレポートを保存"
       "しました。不具合の報告に添付してください。"),
    ZH_HANS("旋影工坊意外退出。报告已保存到下面的文件，请在报告问题时附上它。"),
    ZH_HANT("旋影工坊意外結束。報告已儲存到下面的檔案，回報問題時請附上它。"),
    KO("스피룰라 스튜디오가 예기치 않게 종료되었습니다. 아래 파일에 보고서를 "
       "저장했으니 버그 신고에 첨부해 주세요."),
    DE("Spirula Studio wurde unerwartet beendet. Ein Bericht wurde in der "
       "Datei unten gespeichert; bitte hängen Sie sie an einen Fehlerbericht an."),
    FR("Spirula Studio s'est arrêté de façon inattendue. Un rapport a été "
       "enregistré dans le fichier ci-dessous ; merci de le joindre à un "
       "signalement de bogue."),
    ES("Spirula Studio se cerró de forma inesperada. Se guardó un informe en "
       "el archivo de abajo; adjúntalo al informar del error."),
    PT("O Spirula Studio encerrou de forma inesperada. Um relatório foi salvo "
       "no arquivo abaixo; anexe-o ao relatar o problema."),
    IT("Spirula Studio si è chiuso in modo imprevisto. Un rapporto è stato "
       "salvato nel file qui sotto; allegalo alla segnalazione del problema."),
    NL("Spirula Studio is onverwacht gestopt. Er is een rapport opgeslagen in "
       "het bestand hieronder; voeg het toe aan een foutmelding."),
    RU("Spirula Studio неожиданно завершил работу. Отчёт сохранён в файл "
       "ниже — приложите его к сообщению об ошибке."),
    TR("Spirula Studio beklenmedik biçimde kapandı. Aşağıdaki dosyaya bir "
       "rapor kaydedildi; hata bildirimine ekleyin."));

// ===========================================================================
// Run log -- the settings snapshot at the top of <dataset>/logs/*.log
// ===========================================================================
// Headings only. The key=value lines under them are config field names and
// their values, which stay themselves in every language.

SS_MSG(runlog_section_run,
    EN("Run"),           JA("実行"),          ZH_HANS("运行"),     ZH_HANT("執行"),
    KO("실행"),           DE("Lauf"),         FR("Exécution"),    ES("Ejecución"),
    PT("Execução"),      IT("Esecuzione"),   NL("Run"),          RU("Запуск"),
    TR("Çalıştırma"));

SS_MSG(runlog_section_prep,
    EN("Dataset preparation"),
    JA("データセットの準備"),
    ZH_HANS("数据集准备"),
    ZH_HANT("資料集準備"),
    KO("데이터셋 준비"),
    DE("Datensatzvorbereitung"),
    FR("Préparation du jeu de données"),
    ES("Preparación del conjunto de datos"),
    PT("Preparação do conjunto de dados"),
    IT("Preparazione del set di dati"),
    NL("Datasetvoorbereiding"),
    RU("Подготовка набора данных"),
    TR("Veri kümesi hazırlığı"));

// {0} is the engine's own name (SfM, COLMAP), which is not translated.
SS_MSG(runlog_section_recon,
    EN("Reconstruction ({0})"),
    JA("再構成（{0}）"),
    ZH_HANS("重建（{0}）"),
    ZH_HANT("重建（{0}）"),
    KO("재구성({0})"),
    DE("Rekonstruktion ({0})"),
    FR("Reconstruction ({0})"),
    ES("Reconstrucción ({0})"),
    PT("Reconstrução ({0})"),
    IT("Ricostruzione ({0})"),
    NL("Reconstructie ({0})"),
    RU("Реконструкция ({0})"),
    TR("Yeniden oluşturma ({0})"));

SS_MSG(runlog_section_geometry,
    EN("Geometry (depth and normals)"),
    JA("ジオメトリ（深度と法線）"),
    ZH_HANS("几何（深度与法线）"),
    ZH_HANT("幾何（深度與法線）"),
    KO("지오메트리(깊이와 법선)"),
    DE("Geometrie (Tiefe und Normalen)"),
    FR("Géométrie (profondeur et normales)"),
    ES("Geometría (profundidad y normales)"),
    PT("Geometria (profundidade e normais)"),
    IT("Geometria (profondità e normali)"),
    NL("Geometrie (diepte en normalen)"),
    RU("Геометрия (глубина и нормали)"),
    TR("Geometri (derinlik ve normaller)"));

// {0} is a training section heading -- msg::train::section_label().
SS_MSG(runlog_section_train,
    EN("Training: {0}"),
    JA("学習: {0}"),
    ZH_HANS("训练：{0}"),
    ZH_HANT("訓練：{0}"),
    KO("학습: {0}"),
    DE("Training: {0}"),
    FR("Entraînement : {0}"),
    ES("Entrenamiento: {0}"),
    PT("Treinamento: {0}"),
    IT("Addestramento: {0}"),
    NL("Training: {0}"),
    RU("Обучение: {0}"),
    TR("Eğitim: {0}"));

SS_MSG(runlog_settings_end,
    EN("End of settings"),
    JA("設定ここまで"),
    ZH_HANS("设置结束"),
    ZH_HANT("設定結束"),
    KO("설정 끝"),
    DE("Ende der Einstellungen"),
    FR("Fin des réglages"),
    ES("Fin de la configuración"),
    PT("Fim das configurações"),
    IT("Fine delle impostazioni"),
    NL("Einde van de instellingen"),
    RU("Конец настроек"),
    TR("Ayarların sonu"));


// ===========================================================================
// Batch processing and the two new kinds of preset
// ===========================================================================

SS_MSG(batch_stage_dataset,
    EN("Create dataset"),
    JA("データセットを作成"),
    ZH_HANS("创建数据集"),
    ZH_HANT("建立資料集"),
    KO("데이터셋 생성"),
    DE("Datensatz erstellen"),
    FR("Créer un jeu de données"),
    ES("Crear conjunto de datos"),
    PT("Criar conjunto de dados"),
    IT("Crea set di dati"),
    NL("Dataset maken"),
    RU("Создать набор данных"),
    TR("Veri kümesi oluştur"));
SS_MSG(batch_stage_train,
    EN("Train"),
    JA("学習"),
    ZH_HANS("训练"),
    ZH_HANT("訓練"),
    KO("학습"),
    DE("Trainieren"),
    FR("Entraîner"),
    ES("Entrenar"),
    PT("Treinar"),
    IT("Addestra"),
    NL("Trainen"),
    RU("Обучить"),
    TR("Eğit"));
SS_MSG(batch_stage_mesh,
    EN("Mesh"),
    JA("メッシュ化"),
    ZH_HANS("生成网格"),
    ZH_HANT("產生網格"),
    KO("메시 생성"),
    DE("Mesh erzeugen"),
    FR("Mailler"),
    ES("Generar malla"),
    PT("Gerar malha"),
    IT("Genera mesh"),
    NL("Mesh maken"),
    RU("Построить полигоны"),
    TR("Ağ oluştur"));
SS_MSG(batch_add_create,
    EN("Add videos..."),
    JA("動画を追加…"),
    ZH_HANS("添加视频…"),
    ZH_HANT("新增影片…"),
    KO("동영상 추가…"),
    DE("Videos hinzufügen …"),
    FR("Ajouter des vidéos…"),
    ES("Añadir vídeos…"),
    PT("Adicionar vídeos…"),
    IT("Aggiungi video…"),
    NL("Video's toevoegen…"),
    RU("Добавить видео…"),
    TR("Video ekle…"));
SS_MSG(batch_add_create_help,
    EN("Adds a row that builds a dataset from these files and then trains it. "
       "Either step can be switched off in the row."),
    JA("これらのファイルからデータセットを作り、続けて学習する行を追加します。どちらの工程も行の中で外せます。"),
    ZH_HANS("添加一行：先用这些文件建数据集，再接着训练。两步都可以在行里关掉。"),
    ZH_HANT("新增一列：先用這些檔案建資料集，再接著訓練。兩個步驟都可以在列裡關掉。"),
    KO("이 파일들로 데이터셋을 만들고 이어서 학습하는 행을 추가합니다. 두 단계 모두 행 안에서 끌 수 있습니다."),
    DE("Fügt eine Zeile hinzu, die aus diesen Dateien einen Datensatz baut und "
       "ihn dann trainiert. Beide Schritte lassen sich in der Zeile "
       "abschalten."),
    FR("Ajoute une ligne qui construit un jeu de données à partir de ces "
       "fichiers puis l'entraîne. Chaque étape peut être désactivée dans la "
       "ligne."),
    ES("Añade una fila que construye un conjunto de datos con estos archivos y "
       "luego lo entrena. Cada paso puede desactivarse en la fila."),
    PT("Adiciona uma linha que constrói um conjunto de dados com estes "
       "arquivos e depois o treina. Cada etapa pode ser desligada na linha."),
    IT("Aggiunge una riga che costruisce un set di dati da questi file e poi "
       "lo addestra. Ogni passo può essere disattivato nella riga."),
    NL("Voegt een rij toe die van deze bestanden een dataset bouwt en die "
       "daarna traint. Beide stappen kunnen in de rij worden uitgezet."),
    RU("Добавляет строку, которая соберёт из этих файлов набор данных и затем "
       "обучит его. Любой из шагов можно отключить в самой строке."),
    TR("Bu dosyalardan bir veri kümesi kuran ve ardından onu eğiten bir satır "
       "ekler. Her iki adım da satır içinde kapatılabilir."));
SS_MSG(batch_add_photos,
    EN("Add a photo folder..."),
    JA("写真フォルダーを追加…"),
    ZH_HANS("添加照片文件夹…"),
    ZH_HANT("新增照片資料夾…"),
    KO("사진 폴더 추가…"),
    DE("Fotoordner hinzufügen …"),
    FR("Ajouter un dossier de photos…"),
    ES("Añadir una carpeta de fotos…"),
    PT("Adicionar uma pasta de fotos…"),
    IT("Aggiungi una cartella di foto…"),
    NL("Fotomap toevoegen…"),
    RU("Добавить папку с фотографиями…"),
    TR("Fotoğraf klasörü ekle…"));
SS_MSG(batch_add_mesh,
    EN("Add a model to mesh..."),
    JA("メッシュ化するモデルを追加…"),
    ZH_HANS("添加要生成网格的模型…"),
    ZH_HANT("新增要產生網格的模型…"),
    KO("메시를 만들 모델 추가…"),
    DE("Modell zum Meshen hinzufügen …"),
    FR("Ajouter un modèle à mailler…"),
    ES("Añadir un modelo para mallar…"),
    PT("Adicionar um modelo para malhar…"),
    IT("Aggiungi un modello da trasformare in mesh…"),
    NL("Model toevoegen om te meshen…"),
    RU("Добавить модель для полигонов…"),
    TR("Ağ oluşturulacak model ekle…"));
SS_MSG(batch_pick_source,
    EN("Choose videos"),
    JA("動画を選ぶ"),
    ZH_HANS("选择视频"),
    ZH_HANT("選擇影片"),
    KO("동영상 선택"),
    DE("Videos wählen"),
    FR("Choisir des vidéos"),
    ES("Elegir vídeos"),
    PT("Escolher vídeos"),
    IT("Scegli i video"),
    NL("Video's kiezen"),
    RU("Выберите видео"),
    TR("Video seçin"));
SS_MSG(batch_pick_photos,
    EN("Choose a photo folder"),
    JA("写真フォルダーを選ぶ"),
    ZH_HANS("选择照片文件夹"),
    ZH_HANT("選擇照片資料夾"),
    KO("사진 폴더 선택"),
    DE("Fotoordner wählen"),
    FR("Choisir un dossier de photos"),
    ES("Elegir una carpeta de fotos"),
    PT("Escolher uma pasta de fotos"),
    IT("Scegli una cartella di foto"),
    NL("Fotomap kiezen"),
    RU("Выберите папку с фотографиями"),
    TR("Fotoğraf klasörü seçin"));
SS_MSG(batch_pick_model,
    EN("Choose a model"),
    JA("モデルを選ぶ"),
    ZH_HANS("选择模型"),
    ZH_HANT("選擇模型"),
    KO("모델 선택"),
    DE("Modell wählen"),
    FR("Choisir un modèle"),
    ES("Elegir un modelo"),
    PT("Escolher um modelo"),
    IT("Scegli un modello"),
    NL("Model kiezen"),
    RU("Выберите модель"),
    TR("Model seçin"));
SS_MSG(batch_row_expand_help,
    EN("Show this row's settings."),
    JA("この行の設定を開きます。"),
    ZH_HANS("展开这一行的设置。"),
    ZH_HANT("展開這一列的設定。"),
    KO("이 행의 설정을 펼칩니다."),
    DE("Die Einstellungen dieser Zeile anzeigen."),
    FR("Afficher les réglages de cette ligne."),
    ES("Mostrar los ajustes de esta fila."),
    PT("Mostrar as configurações desta linha."),
    IT("Mostra le impostazioni di questa riga."),
    NL("De instellingen van deze rij tonen."),
    RU("Показать настройки этой строки."),
    TR("Bu satırın ayarlarını gösterir."));
SS_MSG(batch_row_enabled_help,
    EN("Run this row. Unticked, it stays on the list and is passed over."),
    JA("この行を実行します。外しておくと一覧には残り、飛ばされます。"),
    ZH_HANS("运行这一行。取消勾选后它仍留在列表里，只是被跳过。"),
    ZH_HANT("執行這一列。取消勾選後它仍留在清單裡，只是被跳過。"),
    KO("이 행을 실행합니다. 체크를 풀면 목록에는 남고 건너뜁니다."),
    DE("Diese Zeile ausführen. Ohne Haken bleibt sie in der Liste und wird "
       "übersprungen."),
    FR("Exécuter cette ligne. Décochée, elle reste dans la liste et est "
       "ignorée."),
    ES("Ejecutar esta fila. Sin marcar, permanece en la lista y se omite."),
    PT("Executar esta linha. Sem marcar, ela fica na lista e é ignorada."),
    IT("Esegui questa riga. Senza spunta resta nell'elenco e viene saltata."),
    NL("Deze rij uitvoeren. Zonder vinkje blijft hij in de lijst en wordt "
       "overgeslagen."),
    RU("Выполнить эту строку. Без галочки она остаётся в списке и "
       "пропускается."),
    TR("Bu satırı çalıştırır. İşaret kaldırılırsa listede kalır ve atlanır."));
SS_MSG(batch_runs_count,
    EN("Runs: {0}"),
    JA("実行回数: {0}"),
    ZH_HANS("运行次数: {0}"),
    ZH_HANT("執行次數: {0}"),
    KO("실행 횟수: {0}"),
    DE("Läufe: {0}"),
    FR("Exécutions : {0}"),
    ES("Ejecuciones: {0}"),
    PT("Execuções: {0}"),
    IT("Esecuzioni: {0}"),
    NL("Runs: {0}"),
    RU("Запусков: {0}"),
    TR("Çalıştırma: {0}"));
SS_MSG(batch_move_up,
    EN("Move up"),
    JA("上へ"),
    ZH_HANS("上移"),
    ZH_HANT("上移"),
    KO("위로"),
    DE("Nach oben"),
    FR("Monter"),
    ES("Subir"),
    PT("Mover para cima"),
    IT("Sposta su"),
    NL("Omhoog"),
    RU("Вверх"),
    TR("Yukarı taşı"));
SS_MSG(batch_move_down,
    EN("Move down"),
    JA("下へ"),
    ZH_HANS("下移"),
    ZH_HANT("下移"),
    KO("아래로"),
    DE("Nach unten"),
    FR("Descendre"),
    ES("Bajar"),
    PT("Mover para baixo"),
    IT("Sposta giù"),
    NL("Omlaag"),
    RU("Вниз"),
    TR("Aşağı taşı"));
SS_MSG(batch_row_inputs,
    EN("Inputs"),
    JA("入力"),
    ZH_HANS("输入"),
    ZH_HANT("輸入"),
    KO("입력"),
    DE("Eingaben"),
    FR("Entrées"),
    ES("Entradas"),
    PT("Entradas"),
    IT("Ingressi"),
    NL("Invoer"),
    RU("Исходные файлы"),
    TR("Girdiler"));
SS_MSG(batch_add_video,
    EN("Add a video..."),
    JA("動画を追加…"),
    ZH_HANS("添加视频…"),
    ZH_HANT("新增影片…"),
    KO("동영상 추가…"),
    DE("Video hinzufügen …"),
    FR("Ajouter une vidéo…"),
    ES("Añadir un vídeo…"),
    PT("Adicionar um vídeo…"),
    IT("Aggiungi un video…"),
    NL("Video toevoegen…"),
    RU("Добавить видео…"),
    TR("Video ekle…"));
SS_MSG(batch_preset_dataset,
    EN("Dataset preset"),
    JA("データセットのプリセット"),
    ZH_HANS("数据集预设"),
    ZH_HANT("資料集預設"),
    KO("데이터셋 프리셋"),
    DE("Datensatz-Voreinstellung"),
    FR("Préréglage de jeu de données"),
    ES("Ajuste del conjunto de datos"),
    PT("Predefinição do conjunto de dados"),
    IT("Preimpostazione del set di dati"),
    NL("Datasetvoorinstelling"),
    RU("Пресет набора данных"),
    TR("Veri kümesi hazır ayarı"));
SS_MSG(batch_preset_mesh,
    EN("Meshing preset"),
    JA("メッシュ化のプリセット"),
    ZH_HANS("网格预设"),
    ZH_HANT("網格預設"),
    KO("메시 프리셋"),
    DE("Mesh-Voreinstellung"),
    FR("Préréglage de maillage"),
    ES("Ajuste de mallado"),
    PT("Predefinição de malha"),
    IT("Preimpostazione della mesh"),
    NL("Meshvoorinstelling"),
    RU("Пресет полигонов"),
    TR("Ağ hazır ayarı"));
SS_MSG(batch_preset_stock,
    EN("Default settings"),
    JA("既定の設定"),
    ZH_HANS("默认设置"),
    ZH_HANT("預設設定"),
    KO("기본 설정"),
    DE("Standardeinstellungen"),
    FR("Réglages par défaut"),
    ES("Ajustes predeterminados"),
    PT("Configurações padrão"),
    IT("Impostazioni predefinite"),
    NL("Standaardinstellingen"),
    RU("Настройки по умолчанию"),
    TR("Varsayılan ayarlar"));
SS_MSG(batch_dataset_label,
    EN("Dataset folder"),
    JA("データセットのフォルダー"),
    ZH_HANS("数据集文件夹"),
    ZH_HANT("資料集資料夾"),
    KO("데이터셋 폴더"),
    DE("Datensatzordner"),
    FR("Dossier du jeu de données"),
    ES("Carpeta del conjunto de datos"),
    PT("Pasta do conjunto de dados"),
    IT("Cartella del set di dati"),
    NL("Datasetmap"),
    RU("Папка набора данных"),
    TR("Veri kümesi klasörü"));
SS_MSG(batch_dataset_auto_hint,
    EN("created from the inputs above"),
    JA("上の入力から作られます"),
    ZH_HANS("由上面的输入创建"),
    ZH_HANT("由上面的輸入建立"),
    KO("위 입력으로 만들어집니다"),
    DE("wird aus den Eingaben oben erstellt"),
    FR("créé à partir des entrées ci-dessus"),
    ES("se crea con las entradas de arriba"),
    PT("criado a partir das entradas acima"),
    IT("creata dagli ingressi qui sopra"),
    NL("wordt uit de invoer hierboven gemaakt"),
    RU("будет создана из файлов выше"),
    TR("yukarıdaki girdilerden oluşturulur"));
SS_MSG(batch_runs,
    EN("Training runs"),
    JA("学習の実行"),
    ZH_HANS("训练运行"),
    ZH_HANT("訓練執行"),
    KO("학습 실행"),
    DE("Trainingsläufe"),
    FR("Exécutions d'entraînement"),
    ES("Ejecuciones de entrenamiento"),
    PT("Execuções de treinamento"),
    IT("Esecuzioni di addestramento"),
    NL("Trainingsruns"),
    RU("Запуски обучения"),
    TR("Eğitim çalıştırmaları"));
SS_MSG(batch_runs_default,
    EN("No preset chosen: one run on the default settings."),
    JA("プリセット未選択: 既定の設定で 1 回だけ実行します。"),
    ZH_HANS("未选预设：按默认设置运行一次。"),
    ZH_HANT("未選預設：以預設設定執行一次。"),
    KO("프리셋을 고르지 않았습니다: 기본 설정으로 한 번 실행합니다."),
    DE("Keine Voreinstellung gewählt: ein Lauf mit den Standardeinstellungen."),
    FR("Aucun préréglage choisi : une exécution avec les réglages par défaut."),
    ES("Sin ajuste elegido: una ejecución con los valores predeterminados."),
    PT("Nenhuma predefinição escolhida: uma execução com as configurações "
       "padrão."),
    IT("Nessuna preimpostazione scelta: una esecuzione con le impostazioni "
       "predefinite."),
    NL("Geen voorinstelling gekozen: één run met de standaardinstellingen."),
    RU("Пресет не выбран: один запуск с настройками по умолчанию."),
    TR("Hazır ayar seçilmedi: varsayılan ayarlarla tek çalıştırma."));
SS_MSG(batch_add_run,
    EN("Add a run"),
    JA("実行を追加"),
    ZH_HANS("添加一次运行"),
    ZH_HANT("新增一次執行"),
    KO("실행 추가"),
    DE("Lauf hinzufügen"),
    FR("Ajouter une exécution"),
    ES("Añadir una ejecución"),
    PT("Adicionar uma execução"),
    IT("Aggiungi un'esecuzione"),
    NL("Run toevoegen"),
    RU("Добавить запуск"),
    TR("Çalıştırma ekle"));
SS_MSG(batch_add_run_help,
    EN("One more training run over the same dataset, with a preset of its own."),
    JA("同じデータセットをもう一度、別のプリセットで学習します。"),
    ZH_HANS("在同一个数据集上再训练一次，用另一个预设。"),
    ZH_HANT("在同一個資料集上再訓練一次，用另一個預設。"),
    KO("같은 데이터셋을 프리셋만 바꿔 한 번 더 학습합니다."),
    DE("Noch ein Trainingslauf über denselben Datensatz, mit eigener "
       "Voreinstellung."),
    FR("Une exécution d'entraînement de plus sur le même jeu de données, avec "
       "son propre préréglage."),
    ES("Otra ejecución de entrenamiento sobre el mismo conjunto de datos, con "
       "su propio ajuste."),
    PT("Mais uma execução de treinamento sobre o mesmo conjunto de dados, com "
       "sua própria predefinição."),
    IT("Un'altra esecuzione di addestramento sullo stesso set di dati, con una "
       "sua preimpostazione."),
    NL("Nog een trainingsrun over dezelfde dataset, met een eigen "
       "voorinstelling."),
    RU("Ещё один запуск обучения по тому же набору данных, со своим пресетом."),
    TR("Aynı veri kümesi üzerinde, kendi hazır ayarıyla bir eğitim "
       "çalıştırması daha."));
SS_MSG(batch_mesh_model_label,
    EN("Model"),
    JA("モデル"),
    ZH_HANS("模型"),
    ZH_HANT("模型"),
    KO("모델"),
    DE("Modell"),
    FR("Modèle"),
    ES("Modelo"),
    PT("Modelo"),
    IT("Modello"),
    NL("Model"),
    RU("Модель"),
    TR("Model"));
SS_MSG(batch_mesh_model_hint,
    EN("what this row trains"),
    JA("この行が学習したもの"),
    ZH_HANS("这一行训练出来的东西"),
    ZH_HANT("這一列訓練出來的東西"),
    KO("이 행이 학습한 결과"),
    DE("was diese Zeile trainiert"),
    FR("ce que cette ligne entraîne"),
    ES("lo que entrena esta fila"),
    PT("o que esta linha treina"),
    IT("ciò che questa riga addestra"),
    NL("wat deze rij traint"),
    RU("то, что обучит эта строка"),
    TR("bu satırın eğittiği model"));
SS_MSG(batch_mesh_model_pick,
    EN("a run folder or a splat .ply"),
    JA("実行フォルダーまたは splat の .ply"),
    ZH_HANS("运行文件夹或 splat 的 .ply"),
    ZH_HANT("執行資料夾或 splat 的 .ply"),
    KO("실행 폴더 또는 splat .ply"),
    DE("ein Laufordner oder eine Splat-.ply"),
    FR("un dossier d'exécution ou un .ply de splats"),
    ES("una carpeta de ejecución o un .ply de splats"),
    PT("uma pasta de execução ou um .ply de splats"),
    IT("una cartella di esecuzione o un .ply di splat"),
    NL("een runmap of een splat-.ply"),
    RU("папка запуска или .ply со сплатами"),
    TR("bir çalıştırma klasörü ya da splat .ply dosyası"));
SS_MSG(batch_plan_title,
    EN("What will run"),
    JA("実行される内容"),
    ZH_HANS("将要执行的内容"),
    ZH_HANT("將要執行的內容"),
    KO("실행될 작업"),
    DE("Was ausgeführt wird"),
    FR("Ce qui va être exécuté"),
    ES("Lo que se va a ejecutar"),
    PT("O que será executado"),
    IT("Cosa verrà eseguito"),
    NL("Wat er gaat draaien"),
    RU("Что будет выполнено"),
    TR("Ne çalışacak"));
SS_MSG(batch_plan_empty,
    EN("Nothing on the list is switched on."),
    JA("一覧の中に有効な行がありません。"),
    ZH_HANS("列表里没有启用的行。"),
    ZH_HANT("清單裡沒有啟用的列。"),
    KO("목록에 켜진 행이 없습니다."),
    DE("In der Liste ist nichts eingeschaltet."),
    FR("Rien n'est activé dans la liste."),
    ES("No hay nada activado en la lista."),
    PT("Nada na lista está ativado."),
    IT("Nell'elenco non c'è nulla di attivo."),
    NL("Er staat niets aan in de lijst."),
    RU("В списке ничего не включено."),
    TR("Listede açık olan bir şey yok."));
SS_MSG(batch_plan_dataset,
    EN("{0}. Build the dataset in {1}"),
    JA("{0}. {1} にデータセットを作る"),
    ZH_HANS("{0}. 在 {1} 建数据集"),
    ZH_HANT("{0}. 在 {1} 建資料集"),
    KO("{0}. {1} 에 데이터셋 만들기"),
    DE("{0}. Den Datensatz in {1} bauen"),
    FR("{0}. Construire le jeu de données dans {1}"),
    ES("{0}. Construir el conjunto de datos en {1}"),
    PT("{0}. Construir o conjunto de dados em {1}"),
    IT("{0}. Costruire il set di dati in {1}"),
    NL("{0}. De dataset bouwen in {1}"),
    RU("{0}. Собрать набор данных в {1}"),
    TR("{0}. Veri kümesini {1} içinde kur"));
SS_MSG(batch_plan_train,
    EN("{0}. Train {1} with the preset {2}"),
    JA("{0}. {1} をプリセット {2} で学習する"),
    ZH_HANS("{0}. 用预设 {2} 训练 {1}"),
    ZH_HANT("{0}. 用預設 {2} 訓練 {1}"),
    KO("{0}. {1} 을(를) 프리셋 {2} 로 학습"),
    DE("{0}. {1} mit der Voreinstellung {2} trainieren"),
    FR("{0}. Entraîner {1} avec le préréglage {2}"),
    ES("{0}. Entrenar {1} con el ajuste {2}"),
    PT("{0}. Treinar {1} com a predefinição {2}"),
    IT("{0}. Addestrare {1} con la preimpostazione {2}"),
    NL("{0}. {1} trainen met de voorinstelling {2}"),
    RU("{0}. Обучить {1} с пресетом {2}"),
    TR("{0}. {1} kümesini {2} hazır ayarıyla eğit"));
SS_MSG(batch_plan_train_new,
    EN("{0}. Train the dataset this row builds, with the preset {1}"),
    JA("{0}. この行が作るデータセットをプリセット {1} で学習する"),
    ZH_HANS("{0}. 用预设 {1} 训练这一行建出来的数据集"),
    ZH_HANT("{0}. 用預設 {1} 訓練這一列建出來的資料集"),
    KO("{0}. 이 행이 만드는 데이터셋을 프리셋 {1} 로 학습"),
    DE("{0}. Den Datensatz dieser Zeile mit der Voreinstellung {1} trainieren"),
    FR("{0}. Entraîner le jeu de données construit par cette ligne, avec le "
       "préréglage {1}"),
    ES("{0}. Entrenar el conjunto de datos que construye esta fila, con el "
       "ajuste {1}"),
    PT("{0}. Treinar o conjunto de dados que esta linha constrói, com a "
       "predefinição {1}"),
    IT("{0}. Addestrare il set di dati costruito da questa riga, con la "
       "preimpostazione {1}"),
    NL("{0}. De dataset die deze rij bouwt trainen met de voorinstelling {1}"),
    RU("{0}. Обучить набор данных, который соберёт эта строка, с пресетом {1}"),
    TR("{0}. Bu satırın kuracağı veri kümesini {1} hazır ayarıyla eğit"));
SS_MSG(batch_plan_mesh,
    EN("{0}. Mesh {1}"),
    JA("{0}. {1} をメッシュ化する"),
    ZH_HANS("{0}. 为 {1} 生成网格"),
    ZH_HANT("{0}. 為 {1} 產生網格"),
    KO("{0}. {1} 의 메시 생성"),
    DE("{0}. {1} meshen"),
    FR("{0}. Mailler {1}"),
    ES("{0}. Mallar {1}"),
    PT("{0}. Gerar a malha de {1}"),
    IT("{0}. Generare la mesh di {1}"),
    NL("{0}. {1} meshen"),
    RU("{0}. Построить полигоны для {1}"),
    TR("{0}. {1} için ağ oluştur"));
SS_MSG(batch_plan_mesh_new,
    EN("{0}. Mesh what this row trained"),
    JA("{0}. この行が学習したものをメッシュ化する"),
    ZH_HANS("{0}. 为这一行训练出的结果生成网格"),
    ZH_HANT("{0}. 為這一列訓練出的結果產生網格"),
    KO("{0}. 이 행이 학습한 결과의 메시 생성"),
    DE("{0}. Meshen, was diese Zeile trainiert hat"),
    FR("{0}. Mailler ce que cette ligne a entraîné"),
    ES("{0}. Mallar lo que ha entrenado esta fila"),
    PT("{0}. Gerar a malha do que esta linha treinou"),
    IT("{0}. Generare la mesh di ciò che questa riga ha addestrato"),
    NL("{0}. Meshen wat deze rij heeft getraind"),
    RU("{0}. Построить полигоны по тому, что обучила эта строка"),
    TR("{0}. Bu satırın eğittiği modelden ağ oluştur"));
SS_MSG(batch_log_build,
    EN("Batch task {0}: building the dataset in {1}"),
    JA("バッチ作業 {0}: {1} にデータセットを作成中"),
    ZH_HANS("批处理任务 {0}: 正在 {1} 建数据集"),
    ZH_HANT("批次作業 {0}: 正在 {1} 建資料集"),
    KO("일괄 작업 {0}: {1} 에 데이터셋 생성 중"),
    DE("Stapelaufgabe {0}: Datensatz wird in {1} gebaut"),
    FR("Tâche de lot {0} : construction du jeu de données dans {1}"),
    ES("Tarea del lote {0}: construyendo el conjunto de datos en {1}"),
    PT("Tarefa do lote {0}: construindo o conjunto de dados em {1}"),
    IT("Attività del batch {0}: costruzione del set di dati in {1}"),
    NL("Batchtaak {0}: de dataset wordt gebouwd in {1}"),
    RU("Задача пакета {0}: сборка набора данных в {1}"),
    TR("Toplu iş {0}: veri kümesi {1} içinde kuruluyor"));
SS_MSG(batch_log_train,
    EN("Batch task {0}: training {1}"),
    JA("バッチ作業 {0}: {1} を学習中"),
    ZH_HANS("批处理任务 {0}: 正在训练 {1}"),
    ZH_HANT("批次作業 {0}: 正在訓練 {1}"),
    KO("일괄 작업 {0}: {1} 학습 중"),
    DE("Stapelaufgabe {0}: {1} wird trainiert"),
    FR("Tâche de lot {0} : entraînement de {1}"),
    ES("Tarea del lote {0}: entrenando {1}"),
    PT("Tarefa do lote {0}: treinando {1}"),
    IT("Attività del batch {0}: addestramento di {1}"),
    NL("Batchtaak {0}: {1} wordt getraind"),
    RU("Задача пакета {0}: обучение {1}"),
    TR("Toplu iş {0}: {1} eğitiliyor"));
SS_MSG(batch_log_mesh,
    EN("Batch task {0}: meshing {1}"),
    JA("バッチ作業 {0}: {1} をメッシュ化中"),
    ZH_HANS("批处理任务 {0}: 正在为 {1} 生成网格"),
    ZH_HANT("批次作業 {0}: 正在為 {1} 產生網格"),
    KO("일괄 작업 {0}: {1} 메시 생성 중"),
    DE("Stapelaufgabe {0}: {1} wird gemesht"),
    FR("Tâche de lot {0} : maillage de {1}"),
    ES("Tarea del lote {0}: mallando {1}"),
    PT("Tarefa do lote {0}: gerando a malha de {1}"),
    IT("Attività del batch {0}: generazione della mesh di {1}"),
    NL("Batchtaak {0}: {1} wordt gemesht"),
    RU("Задача пакета {0}: построение полигонов для {1}"),
    TR("Toplu iş {0}: {1} için ağ oluşturuluyor"));
SS_MSG(batch_log_task_skipped,
    EN("Batch task {0} was passed over: what it needed was never produced."),
    JA("バッチ作業 {0} を飛ばしました: 必要なものが作られませんでした。"),
    ZH_HANS("跳过批处理任务 {0}: 它需要的东西没有被产出。"),
    ZH_HANT("跳過批次作業 {0}: 它需要的東西沒有被產出。"),
    KO("일괄 작업 {0} 을(를) 건너뛰었습니다: 필요한 것이 만들어지지 않았습니다."),
    DE("Stapelaufgabe {0} übersprungen: was sie brauchte, ist nie entstanden."),
    FR("Tâche de lot {0} ignorée : ce dont elle avait besoin n'a jamais été "
       "produit."),
    ES("Tarea del lote {0} omitida: lo que necesitaba nunca se produjo."),
    PT("Tarefa do lote {0} ignorada: o que ela precisava nunca foi produzido."),
    IT("Attività del batch {0} saltata: ciò che le serviva non è mai stato "
       "prodotto."),
    NL("Batchtaak {0} overgeslagen: wat hij nodig had is nooit gemaakt."),
    RU("Задача пакета {0} пропущена: то, что ей было нужно, не было создано."),
    TR("Toplu iş {0} atlandı: ihtiyaç duyduğu şey hiç üretilmedi."));
SS_MSG(preset_drop_hint_plain,
    EN("Tip: a preset file can be dropped onto this window."),
    JA("ヒント: プリセットのファイルはこのウィンドウにドロップできます。"),
    ZH_HANS("提示：预设文件可以直接拖到这个窗口里。"),
    ZH_HANT("提示：預設檔案可以直接拖到這個視窗裡。"),
    KO("팁: 프리셋 파일은 이 창에 끌어다 놓을 수 있습니다."),
    DE("Tipp: Eine Voreinstellungsdatei lässt sich auf dieses Fenster ziehen."),
    FR("Astuce : un fichier de préréglage peut être déposé sur cette fenêtre."),
    ES("Consejo: un archivo de ajustes se puede soltar sobre esta ventana."),
    PT("Dica: um arquivo de predefinição pode ser solto nesta janela."),
    IT("Suggerimento: un file di preimpostazione può essere trascinato su "
       "questa finestra."),
    NL("Tip: een voorinstellingsbestand kun je op dit venster slepen."),
    RU("Подсказка: файл пресета можно перетащить в это окно."),
    TR("İpucu: bir hazır ayar dosyası bu pencereye bırakılabilir."));
SS_MSG(preset_load_help_plain,
    EN("Read a preset saved earlier. What it is applied TO -- the files, the "
       "output folder -- is left alone."),
    JA("前に保存したプリセットを読み込みます。適用先 (ファイルと出力先フォルダー) はそのままです。"),
    ZH_HANS("读取之前保存的预设。它作用的对象——文件和输出文件夹——保持不变。"),
    ZH_HANT("讀取之前儲存的預設。它作用的對象——檔案與輸出資料夾——保持不變。"),
    KO("이전에 저장한 프리셋을 읽어 옵니다. 적용 대상인 파일과 출력 폴더는 그대로 둡니다."),
    DE("Eine zuvor gespeicherte Voreinstellung lesen. Worauf sie angewendet "
       "wird -- die Dateien, der Ausgabeordner -- bleibt unberührt."),
    FR("Lire un préréglage enregistré plus tôt. Ce à quoi il s'applique -- les "
       "fichiers, le dossier de sortie -- n'est pas touché."),
    ES("Leer un ajuste guardado antes. Aquello a lo que se aplica -- los "
       "archivos, la carpeta de salida -- se deja como está."),
    PT("Ler uma predefinição salva antes. Aquilo a que ela se aplica -- os "
       "arquivos, a pasta de saída -- fica como está."),
    IT("Legge una preimpostazione salvata in precedenza. Ciò a cui viene "
       "applicata -- i file, la cartella di uscita -- resta com'è."),
    NL("Een eerder opgeslagen voorinstelling lezen. Waarop hij wordt toegepast "
       "-- de bestanden, de uitvoermap -- blijft ongemoeid."),
    RU("Прочитать сохранённый ранее пресет. То, к чему он применяется -- файлы "
       "и папка вывода -- остаётся прежним."),
    TR("Daha önce kaydedilmiş bir hazır ayarı okur. Uygulandığı şeyler -- "
       "dosyalar ve çıktı klasörü -- olduğu gibi kalır."));
SS_MSG(chk_nothing_to_do,
    EN("This row has nothing switched on."),
    JA("この行は何も有効になっていません。"),
    ZH_HANS("这一行什么都没启用。"),
    ZH_HANT("這一列什麼都沒啟用。"),
    KO("이 행은 아무것도 켜져 있지 않습니다."),
    DE("In dieser Zeile ist nichts eingeschaltet."),
    FR("Rien n'est activé dans cette ligne."),
    ES("En esta fila no hay nada activado."),
    PT("Nesta linha não há nada ativado."),
    IT("In questa riga non è attivo nulla."),
    NL("In deze rij staat niets aan."),
    RU("В этой строке ничего не включено."),
    TR("Bu satırda hiçbir şey açık değil."));
SS_MSG(chk_sources_empty,
    EN("This row builds a dataset but has nothing to build it from."),
    JA("この行はデータセットを作りますが、材料が指定されていません。"),
    ZH_HANS("这一行要建数据集，却没有可用的素材。"),
    ZH_HANT("這一列要建資料集，卻沒有可用的素材。"),
    KO("이 행은 데이터셋을 만들지만 재료가 없습니다."),
    DE("Diese Zeile baut einen Datensatz, hat aber nichts, woraus."),
    FR("Cette ligne construit un jeu de données mais n'a rien à partir de "
       "quoi."),
    ES("Esta fila construye un conjunto de datos pero no tiene con qué."),
    PT("Esta linha constrói um conjunto de dados mas não tem com o quê."),
    IT("Questa riga costruisce un set di dati ma non ha da cosa."),
    NL("Deze rij bouwt een dataset maar heeft niets om die van te bouwen."),
    RU("Эта строка собирает набор данных, но собирать его не из чего."),
    TR("Bu satır bir veri kümesi kuruyor ama kuracak bir şeyi yok."));
SS_MSG(chk_source_missing,
    EN("This input does not exist: {0}"),
    JA("この入力は存在しません: {0}"),
    ZH_HANS("这个输入不存在: {0}"),
    ZH_HANT("這個輸入不存在: {0}"),
    KO("이 입력은 없습니다: {0}"),
    DE("Diese Eingabe gibt es nicht: {0}"),
    FR("Cette entrée n'existe pas : {0}"),
    ES("Esta entrada no existe: {0}"),
    PT("Esta entrada não existe: {0}"),
    IT("Questo ingresso non esiste: {0}"),
    NL("Deze invoer bestaat niet: {0}"),
    RU("Этого файла нет: {0}"),
    TR("Bu girdi yok: {0}"));
SS_MSG(chk_source_no_images,
    EN("This folder holds no images: {0}"),
    JA("このフォルダーに画像がありません: {0}"),
    ZH_HANS("这个文件夹里没有图像: {0}"),
    ZH_HANT("這個資料夾裡沒有影像: {0}"),
    KO("이 폴더에는 이미지가 없습니다: {0}"),
    DE("Dieser Ordner enthält keine Bilder: {0}"),
    FR("Ce dossier ne contient aucune image : {0}"),
    ES("Esta carpeta no contiene imágenes: {0}"),
    PT("Esta pasta não contém imagens: {0}"),
    IT("Questa cartella non contiene immagini: {0}"),
    NL("Deze map bevat geen afbeeldingen: {0}"),
    RU("В этой папке нет изображений: {0}"),
    TR("Bu klasörde görüntü yok: {0}"));
SS_MSG(chk_source_unsupported,
    EN("This file is not a video this program can read: {0}"),
    JA("このファイルはこのプログラムが読める動画ではありません: {0}"),
    ZH_HANS("这个文件不是本程序能读的视频: {0}"),
    ZH_HANT("這個檔案不是本程式能讀的影片: {0}"),
    KO("이 파일은 이 프로그램이 읽을 수 있는 동영상이 아닙니다: {0}"),
    DE("Diese Datei ist kein Video, das dieses Programm lesen kann: {0}"),
    FR("Ce fichier n'est pas une vidéo que ce programme sait lire : {0}"),
    ES("Este archivo no es un vídeo que este programa pueda leer: {0}"),
    PT("Este arquivo não é um vídeo que este programa consiga ler: {0}"),
    IT("Questo file non è un video che questo programma sappia leggere: {0}"),
    NL("Dit bestand is geen video die dit programma kan lezen: {0}"),
    RU("Этот файл не является видео, которое программа умеет читать: {0}"),
    TR("Bu dosya bu programın okuyabileceği bir video değil: {0}"));
SS_MSG(chk_engine_unavailable,
    EN("The reconstruction this row asks for is not available in this build."),
    JA("この行が求める再構成は、このビルドでは使えません。"),
    ZH_HANS("这一行要用的重建方式在本版本里不可用。"),
    ZH_HANT("這一列要用的重建方式在本版本裡不可用。"),
    KO("이 행이 요구하는 재구성은 이 빌드에서 쓸 수 없습니다."),
    DE("Die Rekonstruktion, die diese Zeile verlangt, gibt es in diesem Build "
       "nicht."),
    FR("La reconstruction demandée par cette ligne n'est pas disponible dans "
       "cette version."),
    ES("La reconstrucción que pide esta fila no está disponible en esta "
       "compilación."),
    PT("A reconstrução que esta linha pede não está disponível nesta "
       "compilação."),
    IT("La ricostruzione richiesta da questa riga non è disponibile in questa "
       "build."),
    NL("De reconstructie die deze rij vraagt, is niet beschikbaar in deze "
       "build."),
    RU("Реконструкция, которую требует эта строка, недоступна в этой сборке."),
    TR("Bu satırın istediği yeniden oluşturma bu derlemede yok."));
SS_MSG(chk_masking_unavailable,
    EN("This build cannot mask: segmentation was not compiled in."),
    JA("このビルドはマスクを作れません。セグメンテーションが組み込まれていません。"),
    ZH_HANS("本版本无法做遮罩：没有编入分割功能。"),
    ZH_HANT("本版本無法做遮罩：沒有編入分割功能。"),
    KO("이 빌드는 마스크를 만들 수 없습니다: 분할 기능이 포함되지 않았습니다."),
    DE("Dieser Build kann nicht maskieren: Die Segmentierung ist nicht "
       "einkompiliert."),
    FR("Cette version ne sait pas masquer : la segmentation n'a pas été "
       "compilée."),
    ES("Esta compilación no puede enmascarar: la segmentación no está "
       "incluida."),
    PT("Esta compilação não consegue mascarar: a segmentação não foi incluída."),
    IT("Questa build non sa mascherare: la segmentazione non è stata "
       "compilata."),
    NL("Deze build kan niet maskeren: segmentatie is niet meegecompileerd."),
    RU("Эта сборка не умеет делать маски: сегментация не включена при "
       "компиляции."),
    TR("Bu derleme maskeleme yapamaz: bölütleme derlemeye dahil edilmedi."));
SS_MSG(chk_mask_no_prompt,
    EN("Masking is on but the preset carries no prompt, and a batch cannot be "
       "prompted with clicks."),
    JA("マスクが有効ですが、プリセットにプロンプトがありません。バッチではクリックで指示できません。"),
    ZH_HANS("遮罩已开启，但预设里没有提示词；批处理里也无法用点击来指定。"),
    ZH_HANT("遮罩已開啟，但預設裡沒有提示詞；批次處理裡也無法用點擊來指定。"),
    KO("마스크가 켜져 있지만 프리셋에 프롬프트가 없습니다. 일괄 처리에서는 클릭으로 지정할 수 없습니다."),
    DE("Maskierung ist an, aber die Voreinstellung trägt keinen Prompt, und "
       "ein Stapel lässt sich nicht per Klick anweisen."),
    FR("Le masquage est activé mais le préréglage ne porte aucune invite, et "
       "un lot ne peut pas être guidé par des clics."),
    ES("El enmascarado está activado pero el ajuste no lleva ninguna "
       "indicación, y un lote no se puede guiar con clics."),
    PT("O mascaramento está ligado mas a predefinição não traz nenhum texto, e "
       "um lote não pode ser guiado por cliques."),
    IT("Il mascheramento è attivo ma la preimpostazione non porta alcun "
       "prompt, e un batch non si può guidare con i clic."),
    NL("Maskeren staat aan maar de voorinstelling bevat geen prompt, en een "
       "batch kan niet met klikken worden aangestuurd."),
    RU("Маскирование включено, но в пресете нет запроса, а кликами пакет "
       "указать нельзя."),
    TR("Maskeleme açık ama hazır ayarda bir istem yok ve toplu işte tıklamayla "
       "yön verilemez."));
SS_MSG(chk_mask_model_missing,
    EN("The segmentation checkpoint has not been downloaded: {0}"),
    JA("セグメンテーションのチェックポイントが未ダウンロードです: {0}"),
    ZH_HANS("分割模型的权重还没有下载: {0}"),
    ZH_HANT("分割模型的權重還沒有下載: {0}"),
    KO("분할 체크포인트를 아직 내려받지 않았습니다: {0}"),
    DE("Der Segmentierungs-Checkpoint wurde nicht heruntergeladen: {0}"),
    FR("Le point de contrôle de segmentation n'a pas été téléchargé : {0}"),
    ES("El punto de control de segmentación no se ha descargado: {0}"),
    PT("O checkpoint de segmentação não foi baixado: {0}"),
    IT("Il checkpoint di segmentazione non è stato scaricato: {0}"),
    NL("Het segmentatie-checkpoint is niet gedownload: {0}"),
    RU("Контрольная точка сегментации не загружена: {0}"),
    TR("Bölütleme kontrol noktası indirilmedi: {0}"));
SS_MSG(chk_geometry_unavailable,
    EN("This build cannot estimate depth and normals."),
    JA("このビルドは深度と法線を推定できません。"),
    ZH_HANS("本版本无法估计深度和法线。"),
    ZH_HANT("本版本無法估計深度與法線。"),
    KO("이 빌드는 깊이와 법선을 추정할 수 없습니다."),
    DE("Dieser Build kann keine Tiefe und Normalen schätzen."),
    FR("Cette version ne sait pas estimer la profondeur et les normales."),
    ES("Esta compilación no puede estimar profundidad ni normales."),
    PT("Esta compilação não consegue estimar profundidade e normais."),
    IT("Questa build non sa stimare profondità e normali."),
    NL("Deze build kan geen diepte en normalen schatten."),
    RU("Эта сборка не умеет оценивать глубину и нормали."),
    TR("Bu derleme derinlik ve normalleri kestiremez."));
SS_MSG(chk_geometry_model_missing,
    EN("The geometry checkpoint has not been downloaded: {0}"),
    JA("ジオメトリのチェックポイントが未ダウンロードです: {0}"),
    ZH_HANS("几何模型的权重还没有下载: {0}"),
    ZH_HANT("幾何模型的權重還沒有下載: {0}"),
    KO("지오메트리 체크포인트를 아직 내려받지 않았습니다: {0}"),
    DE("Der Geometrie-Checkpoint wurde nicht heruntergeladen: {0}"),
    FR("Le point de contrôle de géométrie n'a pas été téléchargé : {0}"),
    ES("El punto de control de geometría no se ha descargado: {0}"),
    PT("O checkpoint de geometria não foi baixado: {0}"),
    IT("Il checkpoint di geometria non è stato scaricato: {0}"),
    NL("Het geometrie-checkpoint is niet gedownload: {0}"),
    RU("Контрольная точка геометрии не загружена: {0}"),
    TR("Geometri kontrol noktası indirilmedi: {0}"));
SS_MSG(chk_dense_model_missing,
    EN("The dense matching checkpoint has not been downloaded: {0}"),
    JA("密な照合のチェックポイントが未ダウンロードです: {0}"),
    ZH_HANS("稠密匹配模型的权重还没有下载: {0}"),
    ZH_HANT("稠密匹配模型的權重還沒有下載: {0}"),
    KO("밀집 매칭 체크포인트를 아직 내려받지 않았습니다: {0}"),
    DE("Der Checkpoint für dichten Abgleich wurde nicht heruntergeladen: {0}"),
    FR("Le point de contrôle d'appariement dense n'a pas été téléchargé : {0}"),
    ES("El punto de control de emparejamiento denso no se ha descargado: {0}"),
    PT("O checkpoint de correspondência densa não foi baixado: {0}"),
    IT("Il checkpoint di corrispondenza densa non è stato scaricato: {0}"),
    NL("Het checkpoint voor dichte koppeling is niet gedownload: {0}"),
    RU("Контрольная точка плотного сопоставления не загружена: {0}"),
    TR("Yoğun eşleştirme kontrol noktası indirilmedi: {0}"));
SS_MSG(chk_dataset_has_model,
    EN("This folder already holds a reconstruction, so the run adds to it "
       "rather than building one: {0}"),
    JA("このフォルダーには既に再構成があります。実行はそれを作り直さず、付け足します: {0}"),
    ZH_HANS("这个文件夹里已经有重建结果，运行会在其基础上补充，而不是重建: {0}"),
    ZH_HANT("這個資料夾裡已經有重建結果，執行會在其基礎上補充，而不是重建: {0}"),
    KO("이 폴더에는 이미 재구성 결과가 있어, 실행은 새로 만들지 않고 덧붙입니다: {0}"),
    DE("Dieser Ordner enthält bereits eine Rekonstruktion; der Lauf ergänzt "
       "sie, statt eine zu bauen: {0}"),
    FR("Ce dossier contient déjà une reconstruction : l'exécution la complète "
       "au lieu d'en construire une : {0}"),
    ES("Esta carpeta ya contiene una reconstrucción: la ejecución la completa "
       "en lugar de construir una: {0}"),
    PT("Esta pasta já contém uma reconstrução: a execução a complementa em vez "
       "de construir uma: {0}"),
    IT("Questa cartella contiene già una ricostruzione: l'esecuzione la "
       "completa invece di costruirne una: {0}"),
    NL("Deze map bevat al een reconstructie: de run vult die aan in plaats van "
       "er een te bouwen: {0}"),
    RU("В этой папке уже есть реконструкция: запуск дополнит её, а не построит "
       "заново: {0}"),
    TR("Bu klasörde zaten bir yeniden oluşturma var: çalıştırma yenisini "
       "kurmak yerine ona ekleme yapar: {0}"));
SS_MSG(chk_capture_is_photos,
    EN("The preset was made for video, but this row's inputs are photographs."),
    JA("このプリセットは動画向けですが、この行の入力は写真です。"),
    ZH_HANS("这个预设是给视频用的，但这一行的输入是照片。"),
    ZH_HANT("這個預設是給影片用的，但這一列的輸入是照片。"),
    KO("이 프리셋은 동영상용인데, 이 행의 입력은 사진입니다."),
    DE("Die Voreinstellung ist für Video gemacht, die Eingaben dieser Zeile "
       "sind aber Fotos."),
    FR("Le préréglage est fait pour la vidéo, mais les entrées de cette ligne "
       "sont des photos."),
    ES("El ajuste está hecho para vídeo, pero las entradas de esta fila son "
       "fotos."),
    PT("A predefinição foi feita para vídeo, mas as entradas desta linha são "
       "fotos."),
    IT("La preimpostazione è fatta per il video, ma gli ingressi di questa "
       "riga sono foto."),
    NL("De voorinstelling is voor video gemaakt, maar de invoer van deze rij "
       "is foto's."),
    RU("Пресет сделан для видео, но на входе этой строки фотографии."),
    TR("Hazır ayar video için yapılmış ama bu satırın girdileri fotoğraf."));
SS_MSG(chk_workspace_empty,
    EN("There is nowhere to put the dataset this row builds."),
    JA("この行が作るデータセットの置き場所がありません。"),
    ZH_HANS("这一行建出来的数据集没有地方放。"),
    ZH_HANT("這一列建出來的資料集沒有地方放。"),
    KO("이 행이 만드는 데이터셋을 둘 곳이 없습니다."),
    DE("Für den Datensatz dieser Zeile gibt es keinen Ort."),
    FR("Il n'y a nulle part où mettre le jeu de données construit par cette "
       "ligne."),
    ES("No hay dónde poner el conjunto de datos que construye esta fila."),
    PT("Não há onde colocar o conjunto de dados que esta linha constrói."),
    IT("Non c'è dove mettere il set di dati costruito da questa riga."),
    NL("Er is geen plek voor de dataset die deze rij bouwt."),
    RU("Некуда положить набор данных, который соберёт эта строка."),
    TR("Bu satırın kuracağı veri kümesini koyacak bir yer yok."));
SS_MSG(chk_dataset_collision,
    EN("Another row builds a dataset into this same folder: {0}"),
    JA("別の行が同じフォルダーにデータセットを作ります: {0}"),
    ZH_HANS("另一行会往同一个文件夹里建数据集: {0}"),
    ZH_HANT("另一列會往同一個資料夾裡建資料集: {0}"),
    KO("다른 행이 같은 폴더에 데이터셋을 만듭니다: {0}"),
    DE("Eine andere Zeile baut einen Datensatz in denselben Ordner: {0}"),
    FR("Une autre ligne construit un jeu de données dans ce même dossier : {0}"),
    ES("Otra fila construye un conjunto de datos en esta misma carpeta: {0}"),
    PT("Outra linha constrói um conjunto de dados nesta mesma pasta: {0}"),
    IT("Un'altra riga costruisce un set di dati in questa stessa cartella: {0}"),
    NL("Een andere rij bouwt een dataset in dezelfde map: {0}"),
    RU("Другая строка собирает набор данных в ту же папку: {0}"),
    TR("Başka bir satır aynı klasöre veri kümesi kuruyor: {0}"));
SS_MSG(chk_dataset_made_later,
    EN("A later row builds this dataset, so it will not be there yet: {0}"),
    JA("このデータセットは後の行が作るため、この時点ではまだありません: {0}"),
    ZH_HANS("这个数据集由后面的行来建，所以那时它还不存在: {0}"),
    ZH_HANT("這個資料集由後面的列來建，所以那時它還不存在: {0}"),
    KO("이 데이터셋은 뒤쪽 행이 만들므로, 그때는 아직 없습니다: {0}"),
    DE("Eine spätere Zeile baut diesen Datensatz, er ist dann noch nicht da: "
       "{0}"),
    FR("Une ligne ultérieure construit ce jeu de données : il ne sera pas "
       "encore là : {0}"),
    ES("Una fila posterior construye este conjunto de datos, así que todavía "
       "no estará: {0}"),
    PT("Uma linha posterior constrói este conjunto de dados, então ele ainda "
       "não estará lá: {0}"),
    IT("Una riga successiva costruisce questo set di dati, quindi non ci sarà "
       "ancora: {0}"),
    NL("Een latere rij bouwt deze dataset, dus die is er dan nog niet: {0}"),
    RU("Этот набор данных собирает более поздняя строка, значит его ещё не "
       "будет: {0}"),
    TR("Bu veri kümesini daha sonraki bir satır kuruyor, o sırada henüz "
       "olmayacak: {0}"));
SS_MSG(chk_mesh_no_model,
    EN("There is nothing to mesh: the row neither names a model nor trains "
       "one."),
    JA("メッシュ化する対象がありません。この行はモデルを指定も学習もしていません。"),
    ZH_HANS("没有可生成网格的对象：这一行既没指定模型，也不训练模型。"),
    ZH_HANT("沒有可產生網格的對象：這一列既沒指定模型，也不訓練模型。"),
    KO("메시를 만들 대상이 없습니다: 이 행은 모델을 지정하지도, 학습하지도 않습니다."),
    DE("Es gibt nichts zu meshen: Die Zeile nennt kein Modell und trainiert "
       "auch keines."),
    FR("Il n'y a rien à mailler : la ligne ne nomme aucun modèle et n'en "
       "entraîne aucun."),
    ES("No hay nada que mallar: la fila ni nombra un modelo ni entrena uno."),
    PT("Não há nada para malhar: a linha não nomeia um modelo nem treina um."),
    IT("Non c'è nulla da trasformare in mesh: la riga non nomina né addestra "
       "un modello."),
    NL("Er is niets om te meshen: de rij noemt geen model en traint er ook "
       "geen."),
    RU("Нечего превращать в полигоны: строка не называет модель и не обучает "
       "её."),
    TR("Ağ oluşturulacak bir şey yok: satır ne bir model belirtiyor ne de "
       "eğitiyor."));
SS_MSG(chk_mesh_model_missing,
    EN("The model to mesh does not exist: {0}"),
    JA("メッシュ化するモデルが存在しません: {0}"),
    ZH_HANS("要生成网格的模型不存在: {0}"),
    ZH_HANT("要產生網格的模型不存在: {0}"),
    KO("메시를 만들 모델이 없습니다: {0}"),
    DE("Das zu meshende Modell gibt es nicht: {0}"),
    FR("Le modèle à mailler n'existe pas : {0}"),
    ES("El modelo que mallar no existe: {0}"),
    PT("O modelo a malhar não existe: {0}"),
    IT("Il modello da trasformare in mesh non esiste: {0}"),
    NL("Het te meshen model bestaat niet: {0}"),
    RU("Модели для построения полигонов нет: {0}"),
    TR("Ağ oluşturulacak model yok: {0}"));
SS_MSG(chk_mesh_no_dataset,
    EN("Meshing has no photographs to work from, so the surface will be the "
       "rougher, density-only kind."),
    JA("メッシュ化に使える写真がないため、密度だけから作る粗い面になります。"),
    ZH_HANS("生成网格时没有照片可用，得到的将是只靠密度的、比较粗糙的表面。"),
    ZH_HANT("產生網格時沒有照片可用，得到的將是只靠密度的、比較粗糙的表面。"),
    KO("메시를 만들 사진이 없어, 밀도만으로 만든 거친 표면이 됩니다."),
    DE("Dem Meshen fehlen die Fotos, also wird die Oberfläche die gröbere, nur "
       "aus der Dichte gewonnene."),
    FR("Le maillage n'a pas de photos à exploiter : la surface sera la version "
       "plus grossière, tirée de la seule densité."),
    ES("El mallado no tiene fotos con las que trabajar: la superficie será la "
       "más tosca, hecha solo con la densidad."),
    PT("A geração de malha não tem fotos com que trabalhar: a superfície será "
       "a mais grosseira, feita só com a densidade."),
    IT("La mesh non ha foto su cui lavorare: la superficie sarà quella più "
       "grezza, ricavata dalla sola densità."),
    NL("Het meshen heeft geen foto's om mee te werken, dus het oppervlak wordt "
       "de ruwere soort, alleen uit de dichtheid."),
    RU("Для построения полигонов нет фотографий, поэтому поверхность будет "
       "более грубой, только по плотности."),
    TR("Ağ oluşturmanın çalışacağı fotoğraf yok, bu yüzden yüzey yalnızca "
       "yoğunluktan çıkan daha kaba türden olacak."));

SS_MSG(batch_busy_elsewhere,
    EN("Something else is running. Wait for it, or stop it, then start the "
       "batch."),
    JA("ほかの処理が動いています。終わるか停止するまで待ってから、バッチを開始してください。"),
    ZH_HANS("有别的任务在跑。等它结束或先停掉，再开始批处理。"),
    ZH_HANT("有別的工作在跑。等它結束或先停掉，再開始批次處理。"),
    KO("다른 작업이 실행 중입니다. 끝나거나 멈춘 뒤에 일괄 처리를 시작하세요."),
    DE("Es läuft schon etwas anderes. Abwarten oder stoppen, dann den Stapel "
       "starten."),
    FR("Autre chose est en cours. Attendez la fin ou arrêtez-la, puis lancez "
       "le lot."),
    ES("Hay otra cosa en marcha. Espera a que acabe o párala, y luego inicia "
       "el lote."),
    PT("Outra coisa está em execução. Espere ou pare, e então inicie o lote."),
    IT("È in corso qualcos'altro. Attendi o fermalo, poi avvia il batch."),
    NL("Er draait al iets anders. Wacht af of stop het, en start dan de batch."),
    RU("Уже выполняется что-то другое. Дождитесь окончания или остановите, а "
       "затем запустите пакет."),
    TR("Başka bir şey çalışıyor. Bitmesini bekleyin ya da durdurun, sonra "
       "toplu işi başlatın."));

SS_MSG(batch_run_mesh_help,
    EN("Mesh what this run produces. A run trained for its appearance can be far too large to mesh; tick the cheaper one trained beside it instead."),
    JA("この実行の結果をメッシュ化します。見た目のために学習した実行はメッシュ化には大きすぎることがあります。その場合は隣で学習した軽いほうにチェックを入れてください。"),
    ZH_HANS("对这次运行的结果做网格。为外观训练的运行往往太大，做不出网格；这时改勾旁边那个更省资源的运行。"),
    ZH_HANT("對這次執行的結果做網格。為外觀訓練的執行往往太大，做不出網格；這時改勾旁邊那個較省資源的執行。"),
    KO("이 실행의 결과로 메시를 만듭니다. 외형을 위해 학습한 실행은 메시로 만들기에 너무 클 수 있으니, 그럴 때는 옆에서 함께 학습한 가벼운 쪽을 선택하세요."),
    DE("Aus dem Ergebnis dieses Laufs ein Netz erzeugen. Ein auf Aussehen trainierter Lauf kann dafür viel zu groß sein; haken Sie dann den daneben trainierten günstigeren an."),
    FR("Mailler ce que produit cette exécution.\n- Une exécution entraînée pour son apparence peut être bien trop lourde à mailler ;\n- cochez plutôt celle, moins coûteuse, entraînée à côté."),
    ES("Generar la malla de lo que produce esta ejecución.\n- Una ejecución entrenada por su apariencia puede ser demasiado grande para mallar;\n- marque entonces la más económica entrenada junto a ella."),
    PT("Gerar a malha do que esta execução produz. Uma execução treinada pela aparência pode ser grande demais para malhar; marque então a mais barata treinada ao lado."),
    IT("Crea la mesh da ciò che produce questa esecuzione.\n- Un'esecuzione addestrata per l'aspetto può essere troppo grande da convertire;\n- spunta invece quella più leggera addestrata accanto."),
    NL("Maak een mesh van wat deze run oplevert. Een run die op uiterlijk is getraind kan veel te groot zijn om te meshen; vink dan de goedkopere run ernaast aan."),
    RU("Строить меш по результату этого прогона. Прогон, обученный ради внешнего вида, может быть слишком велик для меша; тогда отметьте более дешёвый, обученный рядом."),
    TR("Bu çalıştırmanın sonucundan ağ oluştur. Görünüm için eğitilen bir çalıştırma ağ oluşturmak için fazla büyük olabilir; o zaman yanında eğitilen ucuz olanı işaretleyin."));

SS_MSG(batch_mesh_override_help,
    EN("What this row writes, whatever its preset says. Tick nothing to leave "
       "the preset alone."),
    JA("プリセットの指定にかかわらず、この行が書き出すものです。何もチェックしなければプリセットのままになります。"),
    ZH_HANS("不管预设怎么说，这一行要写出的东西。什么都不勾就按预设来。"),
    ZH_HANT("不管預設怎麼說，這一列要寫出的東西。什麼都不勾就照預設。"),
    KO("프리셋이 무엇이라 하든 이 행이 써 내보낼 것. 아무것도 선택하지 않으면 프리셋 그대로입니다."),
    DE("Was diese Zeile schreibt, unabhängig von ihrer Voreinstellung. Nichts "
       "anhaken lässt die Voreinstellung unberührt."),
    FR("Ce que cette ligne écrit, quoi que dise son préréglage. Ne rien cocher "
       "laisse le préréglage tel quel."),
    ES("Lo que escribe esta fila, diga lo que diga su ajuste. No marcar nada "
       "deja el ajuste como está."),
    PT("O que esta linha escreve, diga o que disser a sua predefinição. Não "
       "marcar nada deixa a predefinição como está."),
    IT("Ciò che questa riga scrive, qualunque cosa dica la sua "
       "preimpostazione. Non spuntare nulla la lascia intatta."),
    NL("Wat deze rij schrijft, wat de voorinstelling ook zegt. Niets aanvinken "
       "laat de voorinstelling met rust."),
    RU("Что записывает эта строка, что бы ни говорил её пресет. Если ничего не "
       "отмечено, пресет остаётся как есть."),
    TR("Hazır ayarı ne derse desin, bu satırın yazacağı şey. Hiçbirini "
       "işaretlemezseniz hazır ayar olduğu gibi kalır."));

SS_MSG(memory_chart_host,
    EN("System memory over the run (GiB)"),
    JA("処理中のシステムメモリ（GiB）"),
    ZH_HANS("运行期间的系统内存（GiB）"),
    ZH_HANT("執行期間的系統記憶體（GiB）"),
    KO("실행 중 시스템 메모리 (GiB)"),
    DE("Arbeitsspeicher im Verlauf (GiB)"),
    FR("Mémoire système au fil de l'exécution (GiB)"),
    ES("Memoria del sistema durante la ejecución (GiB)"),
    PT("Memória do sistema durante a execução (GiB)"),
    IT("Memoria di sistema durante l'esecuzione (GiB)"),
    NL("Systeemgeheugen tijdens de run (GiB)"),
    RU("Системная память во время запуска (GiB)"),
    TR("Çalıştırma boyunca sistem belleği (GiB)"));

SS_MSG(memory_chart_device,
    EN("GPU memory over the run (GiB)"),
    JA("処理中の GPU メモリ（GiB）"),
    ZH_HANS("运行期间的 GPU 内存（GiB）"),
    ZH_HANT("執行期間的 GPU 記憶體（GiB）"),
    KO("실행 중 GPU 메모리 (GiB)"),
    DE("GPU-Speicher im Verlauf (GiB)"),
    FR("Mémoire GPU au fil de l'exécution (GiB)"),
    ES("Memoria GPU durante la ejecución (GiB)"),
    PT("Memória da GPU durante a execução (GiB)"),
    IT("Memoria GPU durante l'esecuzione (GiB)"),
    NL("GPU-geheugen tijdens de run (GiB)"),
    RU("Память GPU во время запуска (GiB)"),
    TR("Çalıştırma boyunca GPU belleği (GiB)"));

SS_MSG(memory_legend_run,
    EN("this run"),
    JA("この処理"),
    ZH_HANS("本次运行"),
    ZH_HANT("本次執行"),
    KO("이번 실행"),
    DE("dieser Lauf"),
    FR("cette exécution"),
    ES("esta ejecución"),
    PT("esta execução"),
    IT("questa esecuzione"),
    NL("deze run"),
    RU("этот запуск"),
    TR("bu çalıştırma"));

SS_MSG(memory_risk_low,
    EN("Memory risk: low"),
    JA("メモリ不足リスク: 低"),
    ZH_HANS("内存不足风险：低"),
    ZH_HANT("記憶體不足風險：低"),
    KO("메모리 부족 위험: 낮음"),
    DE("Speicherrisiko: niedrig"),
    FR("Risque mémoire : faible"),
    ES("Riesgo de memoria: bajo"),
    PT("Risco de memória: baixo"),
    IT("Rischio memoria: basso"),
    NL("Geheugenrisico: laag"),
    RU("Риск нехватки памяти: низкий"),
    TR("Bellek riski: düşük"));

SS_MSG(memory_risk_medium,
    EN("Memory risk: medium"),
    JA("メモリ不足リスク: 中"),
    ZH_HANS("内存不足风险：中"),
    ZH_HANT("記憶體不足風險：中"),
    KO("메모리 부족 위험: 보통"),
    DE("Speicherrisiko: mittel"),
    FR("Risque mémoire : moyen"),
    ES("Riesgo de memoria: medio"),
    PT("Risco de memória: médio"),
    IT("Rischio memoria: medio"),
    NL("Geheugenrisico: middel"),
    RU("Риск нехватки памяти: средний"),
    TR("Bellek riski: orta"));

SS_MSG(memory_risk_high,
    EN("Memory risk: high"),
    JA("メモリ不足リスク: 高"),
    ZH_HANS("内存不足风险：高"),
    ZH_HANT("記憶體不足風險：高"),
    KO("메모리 부족 위험: 높음"),
    DE("Speicherrisiko: hoch"),
    FR("Risque mémoire : élevé"),
    ES("Riesgo de memoria: alto"),
    PT("Risco de memória: alto"),
    IT("Rischio memoria: alto"),
    NL("Geheugenrisico: hoog"),
    RU("Риск нехватки памяти: высокий"),
    TR("Bellek riski: yüksek"));

SS_MSG(memory_forecast_summary,
    EN("In use {0} GiB   projected peak {1} ± {2} GiB   within reach {3} GiB   chance of running out {4}%"),
    JA("使用中 {0} GiB   予測ピーク {1} ± {2} GiB   使える量 {3} GiB   不足する確率 {4}%"),
    ZH_HANS("使用中 {0} GiB   预测峰值 {1} ± {2} GiB   可用 {3} GiB   不足的概率 {4}%"),
    ZH_HANT("使用中 {0} GiB   預測峰值 {1} ± {2} GiB   可用 {3} GiB   不足的機率 {4}%"),
    KO("사용 중 {0} GiB   예측 최대치 {1} ± {2} GiB   사용 가능 {3} GiB   부족할 확률 {4}%"),
    DE("Belegt {0} GiB   Prognose Spitze {1} ± {2} GiB   erreichbar {3} GiB   Wahrscheinlichkeit, dass es nicht reicht: {4} %"),
    FR("Utilisé {0} Gio   pic prévu {1} ± {2} Gio   accessible {3} Gio   risque de manquer {4} %"),
    ES("En uso {0} GiB   pico previsto {1} ± {2} GiB   disponible {3} GiB   probabilidad de quedarse sin memoria {4}%"),
    PT("Em uso {0} GiB   pico previsto {1} ± {2} GiB   disponível {3} GiB   chance de faltar {4}%"),
    IT("In uso {0} GiB   picco previsto {1} ± {2} GiB   disponibile {3} GiB   probabilità di esaurirla {4}%"),
    NL("In gebruik {0} GiB   verwachte piek {1} ± {2} GiB   beschikbaar {3} GiB   kans op tekort {4}%"),
    RU("Занято {0} ГиБ   ожидаемый пик {1} ± {2} ГиБ   доступно {3} ГиБ   вероятность нехватки {4}%"),
    TR("Kullanımda {0} GiB   öngörülen tepe {1} ± {2} GiB   erişilebilir {3} GiB   yetmeme olasılığı %{4}"));

SS_MSG(mesh_memory_help,
    EN("Solid: current use; faint: estimated peak; grey: other apps. High risk: raise surface detail, lower texture size, or close other apps."),
    JA("濃色は現在の使用量、淡色は予測ピーク、灰色は他のアプリです。危険度が高ければ表面詳細の値を上げるか、テクスチャを縮小するか、他のアプリを閉じてください。"),
    ZH_HANS("实色：当前用量；浅色：预计峰值；灰色：其他应用。风险高时，调高表面细节参数、减小纹理，或关闭其他应用。"),
    ZH_HANT("實色：目前用量；淺色：預計峰值；灰色：其他應用程式。風險高時，調高表面細節參數、縮小紋理，或關閉其他應用程式。"),
    KO("진한 색: 현재 사용량, 연한 색: 예상 최대치, 회색: 다른 앱. 위험이 높으면 표면 세부 설정값을 높이거나 텍스처를 줄이거나 다른 앱을 닫으세요."),
    DE("Kräftig: aktuelle Nutzung; blass: geschätzte Spitze; grau: andere Apps. Bei hohem Risiko Oberflächendetail erhöhen, Textur verkleinern oder andere Apps schließen."),
    FR("Plein : usage actuel ; pâle : pic estimé ; gris : autres applis. Risque élevé : augmenter le détail de surface, réduire la texture ou fermer les autres applis."),
    ES("Sólido: uso actual; tenue: pico estimado; gris: otras apps. Si el riesgo es alto, sube el detalle de superficie, reduce la textura o cierra otras apps."),
    PT("Sólido: uso atual; claro: pico estimado; cinzento: outras apps. Com risco alto, aumente o detalhe de superfície, reduza a textura ou feche outras apps."),
    IT("Pieno: uso attuale; tenue: picco stimato; grigio: altre app. Rischio alto: aumenta il dettaglio di superficie, riduci la texture o chiudi altre app."),
    NL("Vol: huidig gebruik; vaag: geschatte piek; grijs: andere apps. Hoog risico: oppervlaktedetail verhogen, textuur verkleinen of andere apps sluiten."),
    RU("Яркий: занято; бледный: прогноз пика; серый: другие приложения. Высокий риск: повысьте детализацию поверхности, уменьшите текстуру или закройте другие приложения."),
    TR("Dolu: mevcut kullanım; soluk: tahmini tepe; gri: diğer uygulamalar. Risk yüksekse yüzey ayrıntısı değerini artırın, dokuyu küçültün veya diğer uygulamaları kapatın."));

SS_MSG(sfm_memory_help,
    EN("Solid: current use; faint: estimated peak; grey: other apps. High risk: use fewer images or features, or close other apps."),
    JA("濃色は現在の使用量、淡色は予測ピーク、灰色は他のアプリです。危険度が高ければ画像や特徴点を減らすか、他のアプリを閉じてください。"),
    ZH_HANS("实色：当前用量；浅色：预计峰值；灰色：其他应用。风险高时，减少图像或特征点，或关闭其他应用。"),
    ZH_HANT("實色：目前用量；淺色：預計峰值；灰色：其他應用程式。風險高時，減少影像或特徵點，或關閉其他應用程式。"),
    KO("진한 색: 현재 사용량, 연한 색: 예상 최대치, 회색: 다른 앱. 위험이 높으면 이미지나 특징점 수를 줄이거나 다른 앱을 닫으세요."),
    DE("Kräftig: aktuelle Nutzung; blass: geschätzte Spitze; grau: andere Apps. Bei hohem Risiko Bilder oder Merkmale reduzieren oder andere Apps schließen."),
    FR("Plein : usage actuel ; pâle : pic estimé ; gris : autres applis. Risque élevé : réduire les images ou les points clés, ou fermer les autres applis."),
    ES("Sólido: uso actual; tenue: pico estimado; gris: otras apps. Si el riesgo es alto, reduce imágenes o puntos clave, o cierra otras apps."),
    PT("Sólido: uso atual; claro: pico estimado; cinzento: outras apps. Com risco alto, reduza imagens ou pontos-chave, ou feche outras apps."),
    IT("Pieno: uso attuale; tenue: picco stimato; grigio: altre app. Se il rischio è alto, riduci immagini o punti chiave, o chiudi altre app."),
    NL("Vol: huidig gebruik; vaag: geschatte piek; grijs: andere apps. Hoog risico: minder beelden of kenmerken gebruiken, of andere apps sluiten."),
    RU("Яркий: текущее потребление; бледный: прогноз пика; серый: другие приложения. При высоком риске уменьшите число снимков или признаков либо закройте другие приложения."),
    TR("Dolu: mevcut kullanım; soluk: tahmini tepe; gri: diğer uygulamalar. Risk yüksekse görüntü veya özellik sayısını azaltın ya da diğer uygulamaları kapatın."));

SS_MSG(progress_time,
    EN("elapsed {0}   ETA {1}"),
    JA("経過 {0}   残り {1}"),
    ZH_HANS("已用 {0}   剩余 {1}"),
    ZH_HANT("已用 {0}   剩餘 {1}"),
    KO("경과 {0}   남은 시간 {1}"),
    DE("Laufzeit {0}   Restzeit {1}"),
    FR("écoulé {0}   reste {1}"),
    ES("lleva {0}   faltan {1}"),
    PT("decorrido {0}   faltam {1}"),
    IT("trascorso {0}   mancano {1}"),
    NL("verstreken {0}   nog {1}"),
    RU("прошло {0}   осталось {1}"),
    TR("geçen {0}   kalan {1}"));

SS_MSG(progress_time_help,
    EN("Elapsed and estimated remaining time. The bar follows recent speed and never moves back; --:--:-- means there is not enough data yet."),
    JA("経過時間と推定残り時間です。バーは最近の速度に基づき、後退しません。--:--:-- は推定に必要なデータが不足している状態です。"),
    ZH_HANS("已用时间和预计剩余时间。进度按近期速度估算，不会倒退；--:--:-- 表示数据还不足以估算。"),
    ZH_HANT("已用時間和預計剩餘時間。進度按近期速度估算，不會倒退；--:--:-- 表示資料還不足以估算。"),
    KO("경과 시간과 예상 남은 시간입니다. 최근 속도로 계산하며 막대는 뒤로 가지 않습니다. --:--:--는 아직 데이터가 부족하다는 뜻입니다."),
    DE("Vergangene und geschätzte Restzeit. Der Balken folgt dem jüngsten Tempo und geht nie zurück; --:--:-- bedeutet, dass noch Daten fehlen."),
    FR("Temps écoulé et restant estimé. La barre suit la vitesse récente sans reculer ; --:--:-- indique que les données sont encore insuffisantes."),
    ES("Tiempo transcurrido y restante estimado. La barra sigue la velocidad reciente y no retrocede; --:--:-- indica que aún faltan datos."),
    PT("Tempo decorrido e restante estimado. A barra segue a velocidade recente e nunca recua; --:--:-- indica que ainda faltam dados."),
    IT("Tempo trascorso e residuo stimato. La barra segue la velocità recente e non arretra; --:--:-- indica che i dati non bastano ancora."),
    NL("Verstreken en geschatte resterende tijd. De balk volgt het recente tempo en gaat nooit terug; --:--:-- betekent dat er nog te weinig gegevens zijn."),
    RU("Прошедшее и ожидаемое оставшееся время. Полоса следует недавней скорости и не отступает; --:--:-- означает, что данных пока мало."),
    TR("Geçen ve tahmini kalan süre. Çubuk son hıza göre ilerler, geri gitmez; --:--:-- henüz yeterli veri olmadığını gösterir."));

SS_MSG(mesh_color_help,
    EN("Tick more than one to get the same surface several ways. It is "
       "extracted once either way; only the color is redone."),
    JA("複数にチェックすると、同じ面をいくつもの形で書き出せます。面の抽出は一度きりで、やり直すのは色だけです。"),
    ZH_HANS("可以勾多项，把同一个面以多种方式写出。面只提取一次，重做的只有颜色。"),
    ZH_HANT("可以勾多項，把同一個面以多種方式寫出。面只擷取一次，重做的只有顏色。"),
    KO("여러 개를 선택하면 같은 표면을 여러 방식으로 얻습니다. 표면은 한 번만 추출하고, 다시 하는 것은 색뿐입니다."),
    DE("Mehrere anhaken, um dieselbe Oberfläche mehrfach zu erhalten. Sie wird "
       "ohnehin nur einmal extrahiert; wiederholt wird nur die Farbe."),
    FR("Cochez-en plusieurs pour obtenir la même surface de plusieurs façons. "
       "Elle n'est extraite qu'une fois ; seule la couleur est refaite."),
    ES("Marque varias para obtener la misma superficie de varias formas. Se "
       "extrae una sola vez; solo se rehace el color."),
    PT("Marque várias para obter a mesma superfície de várias formas. Ela é "
       "extraída uma só vez; apenas a cor é refeita."),
    IT("Spuntane più di una per ottenere la stessa superficie in più modi. "
       "Viene estratta una volta sola; si rifà solo il colore."),
    NL("Vink er meerdere aan om hetzelfde oppervlak op meerdere manieren te "
       "krijgen. Het wordt maar één keer geëxtraheerd; alleen de kleur wordt "
       "opnieuw gedaan."),
    RU("Отметьте несколько, чтобы получить одну и ту же поверхность в разных "
       "видах. Извлекается она всё равно один раз; заново делается только "
       "цвет."),
    TR("Aynı yüzeyi birkaç biçimde almak için birden fazlasını işaretleyin. "
       "Yüzey yine bir kez çıkarılır; yalnızca renk yeniden yapılır."));

SS_MSG(mesh_no_output_warn,
    EN("Nothing would be written: no ticked format can carry a ticked color."),
    JA("何も書き出せません。チェックした形式のどれも、チェックした色を持てません。"),
    ZH_HANS("什么都写不出：勾选的格式都装不下勾选的颜色。"),
    ZH_HANT("什麼都寫不出：勾選的格式都裝不下勾選的顏色。"),
    KO("아무것도 쓰이지 않습니다. 선택한 형식 중 선택한 색을 담을 수 있는 것이 없습니다."),
    DE("Es würde nichts geschrieben: kein angehaktes Format kann eine "
       "angehakte Farbe tragen."),
    FR("Rien ne serait écrit : aucun format coché ne peut porter une couleur "
       "cochée."),
    ES("No se escribiría nada: ningún formato marcado puede llevar un color "
       "marcado."),
    PT("Nada seria escrito: nenhum formato marcado consegue levar uma cor "
       "marcada."),
    IT("Non verrebbe scritto nulla: nessun formato spuntato può portare un "
       "colore spuntato."),
    NL("Er zou niets worden geschreven: geen aangevinkt formaat kan een "
       "aangevinkte kleur dragen."),
    RU("Ничего не будет записано: ни один отмеченный формат не несёт "
       "отмеченный цвет."),
    TR("Hiçbir şey yazılmaz: işaretli biçimlerin hiçbiri işaretli bir rengi "
       "taşıyamaz."));

SS_MSG(chk_mesh_no_runs,
    EN("Meshing is on, but no training run is ticked for it."),
    JA("メッシュ化が有効ですが、対象の学習実行が一つも選ばれていません。"),
    ZH_HANS("开了网格生成，但没有勾选任何训练运行。"),
    ZH_HANT("開了網格生成，但沒有勾選任何訓練執行。"),
    KO("메시 생성이 켜져 있지만, 대상으로 선택된 학습 실행이 없습니다."),
    DE("Netzerzeugung ist an, aber kein Trainingslauf ist dafür angehakt."),
    FR("Le maillage est activé, mais aucune exécution d'entraînement n'est "
       "cochée pour lui."),
    ES("El mallado está activado, pero no hay ninguna ejecución de "
       "entrenamiento marcada."),
    PT("A malha está ativada, mas nenhuma execução de treino está marcada para "
       "ela."),
    IT("La mesh è attiva, ma nessuna esecuzione di addestramento è spuntata "
       "per essa."),
    NL("Meshen staat aan, maar er is geen trainingsrun voor aangevinkt."),
    RU("Построение меша включено, но ни один прогон обучения для него не "
       "отмечен."),
    TR("Ağ oluşturma açık, ama bunun için hiçbir eğitim çalıştırması "
       "işaretlenmedi."));

SS_MSG(chk_mesh_no_output,
    EN("The colors and formats ticked for meshing have no combination between "
       "them."),
    JA("メッシュ化で選んだ色と形式に、組み合わせられるものがありません。"),
    ZH_HANS("为网格生成勾选的颜色和格式之间没有可用的组合。"),
    ZH_HANT("為網格生成勾選的顏色與格式之間沒有可用的組合。"),
    KO("메시 생성에 선택한 색과 형식 사이에 가능한 조합이 없습니다."),
    DE("Die für die Netzerzeugung angehakten Farben und Formate haben keine "
       "gemeinsame Kombination."),
    FR("Les couleurs et les formats cochés pour le maillage n'ont aucune "
       "combinaison possible."),
    ES("Los colores y formatos marcados para el mallado no tienen ninguna "
       "combinación posible."),
    PT("As cores e os formatos marcados para a malha não têm nenhuma "
       "combinação possível."),
    IT("I colori e i formati spuntati per la mesh non hanno alcuna "
       "combinazione possibile."),
    NL("De voor het meshen aangevinkte kleuren en formaten hebben geen enkele "
       "combinatie."),
    RU("У отмеченных для меша цветов и форматов нет ни одной допустимой "
       "комбинации."),
    TR("Ağ oluşturma için işaretlenen renkler ve biçimler arasında hiçbir "
       "birleşim yok."));

SS_MSG(batch_plan_mesh_run,
    EN("{0}. Mesh the run trained with {1}"),
    JA("{0}. {1} で学習した実行をメッシュ化する"),
    ZH_HANS("{0}. 为用 {1} 训练的那次运行生成网格"),
    ZH_HANT("{0}. 為用 {1} 訓練的那次執行產生網格"),
    KO("{0}. {1} 로 학습한 실행으로 메시 만들기"),
    DE("{0}. Den mit {1} trainierten Lauf vernetzen"),
    FR("{0}. Mailler l'exécution entraînée avec {1}"),
    ES("{0}. Mallar la ejecución entrenada con {1}"),
    PT("{0}. Malhar a execução treinada com {1}"),
    IT("{0}. Crea la mesh dell'esecuzione addestrata con {1}"),
    NL("{0}. Mesh maken van de run getraind met {1}"),
    RU("{0}. Построить меш по прогону, обученному с {1}"),
    TR("{0}. {1} ile eğitilen çalıştırmadan ağ oluştur"));


// ===========================================================================
// The navigation gizmo
// ===========================================================================

SS_MSG(gizmo_help,
    EN("Drag to orbit. Click an axis to look along it; click it again for the far side."),
    JA("ドラッグで視点を回転します。軸をクリックするとその軸方向から見ます。もう一度クリックすると反対側からになります。"),
    ZH_HANS("拖动以环绕视角。点击某个轴可沿该轴观察，再点一次则从另一侧观察。"),
    ZH_HANT("拖曳以環繞視角。點選某個軸可沿該軸觀看，再點一次則從另一側觀看。"),
    KO("드래그하면 시점이 회전합니다. 축을 클릭하면 그 축 방향에서 보고, 다시 클릭하면 반대쪽에서 봅니다."),
    DE("Ziehen dreht die Ansicht. Ein Klick auf eine Achse blickt entlang dieser Achse, ein zweiter Klick von der Gegenseite."),
    FR("Faites glisser pour tourner autour. Cliquez sur un axe pour regarder le long de celui-ci, et une seconde fois pour le côté opposé."),
    ES("Arrastra para orbitar. Haz clic en un eje para mirar a lo largo de él; otro clic para el lado opuesto."),
    PT("Arraste para orbitar. Clique em um eixo para olhar ao longo dele; clique de novo para o lado oposto."),
    IT("Trascina per orbitare. Fai clic su un asse per guardare lungo di esso; un altro clic per il lato opposto."),
    NL("Sleep om rond het model te draaien. Klik op een as om erlangs te kijken; klik nogmaals voor de andere kant."),
    RU("Перетащите, чтобы вращать вид. Щёлкните по оси, чтобы смотреть вдоль неё; ещё раз — с обратной стороны."),
    TR("Yörüngede dönmek için sürükleyin. Bir eksene tıklayınca o eksen boyunca bakılır; yeniden tıklayınca karşı taraftan."));

SS_MSG(gizmo_zoom_help,
    EN("Drag up or down to zoom."),
    JA("上下にドラッグしてズームします。"),
    ZH_HANS("上下拖动以缩放。"),
    ZH_HANT("上下拖曳以縮放。"),
    KO("위아래로 드래그해 확대·축소합니다."),
    DE("Zum Zoomen nach oben oder unten ziehen."),
    FR("Faites glisser vers le haut ou le bas pour zoomer."),
    ES("Arrastra hacia arriba o abajo para acercar o alejar."),
    PT("Arraste para cima ou para baixo para aproximar ou afastar."),
    IT("Trascina in alto o in basso per ingrandire o ridurre."),
    NL("Sleep omhoog of omlaag om te zoomen."),
    RU("Перетащите вверх или вниз, чтобы приблизить или отдалить."),
    TR("Yakınlaştırmak için yukarı ya da aşağı sürükleyin."));

SS_MSG(gizmo_pan_help,
    EN("Drag to pan."),
    JA("ドラッグして視点を平行移動します。"),
    ZH_HANS("拖动以平移视角。"),
    ZH_HANT("拖曳以平移視角。"),
    KO("드래그해 시점을 평행 이동합니다."),
    DE("Zum Verschieben der Ansicht ziehen."),
    FR("Faites glisser pour déplacer la vue."),
    ES("Arrastra para desplazar la vista."),
    PT("Arraste para deslocar a vista."),
    IT("Trascina per spostare la vista."),
    NL("Sleep om het beeld te verschuiven."),
    RU("Перетащите, чтобы сдвинуть вид."),
    TR("Görünümü kaydırmak için sürükleyin."));

SS_MSG(gizmo_to_ortho,
    EN("Switch to the orthographic view (numeric-pad 5)."),
    JA("平行投影に切り替えます（テンキーの 5）。"),
    ZH_HANS("切换到正交视图（数字键盘 5）。"),
    ZH_HANT("切換到正交視圖（數字鍵盤 5）。"),
    KO("직교 투영으로 전환합니다(숫자 패드 5)."),
    DE("Zur orthografischen Ansicht wechseln (Ziffernblock 5)."),
    FR("Passer à la vue orthographique (pavé numérique 5)."),
    ES("Cambiar a la vista ortográfica (teclado numérico 5)."),
    PT("Mudar para a vista ortográfica (teclado numérico 5)."),
    IT("Passa alla vista ortografica (tastierino numerico 5)."),
    NL("Overschakelen naar orthografische weergave (numeriek toetsenblok 5)."),
    RU("Переключиться на ортографический вид (цифровая клавиатура 5)."),
    TR("Ortografik görünüme geç (sayısal tuş takımı 5)."));

SS_MSG(gizmo_to_perspective,
    EN("Restore the previous projection (numeric-pad 5)."),
    JA("前の投影に戻します（テンキーの 5）。"),
    ZH_HANS("恢复上次的投影（数字键盘 5）。"),
    ZH_HANT("恢復上次的投影（數字鍵盤 5）。"),
    KO("이전 투영으로 돌아갑니다(숫자 패드 5)."),
    DE("Vorherige Projektion wiederherstellen (Ziffernblock 5)."),
    FR("Rétablir la projection précédente (pavé numérique 5)."),
    ES("Restaurar la proyección anterior (teclado numérico 5)."),
    PT("Restaurar a projeção anterior (teclado numérico 5)."),
    IT("Ripristina la proiezione precedente (tastierino numerico 5)."),
    NL("Vorige projectie herstellen (numeriek toetsenblok 5)."),
    RU("Вернуть предыдущую проекцию (цифровая клавиатура 5)."),
    TR("Önceki projeksiyona dön (sayısal tuş takımı 5)."));



// ===========================================================================
// Saving under a name that gained its extension
// ===========================================================================

SS_MSG(fd_replace_title,
    EN("Replace the file?"),
    JA("ファイルを置き換えますか？"),
    ZH_HANS("要替换文件吗？"),
    ZH_HANT("要取代檔案嗎？"),
    KO("파일을 바꿀까요?"),
    DE("Datei ersetzen?"),
    FR("Remplacer le fichier ?"),
    ES("¿Reemplazar el archivo?"),
    PT("Substituir o arquivo?"),
    IT("Sostituire il file?"),
    NL("Bestand vervangen?"),
    RU("Заменить файл?"),
    TR("Dosya değiştirilsin mi?"));

SS_MSG(fd_replace_body,
    EN("{0} already exists. Replace it?"),
    JA("{0} は既に存在します。置き換えますか？"),
    ZH_HANS("{0} 已经存在。要替换它吗？"),
    ZH_HANT("{0} 已經存在。要取代它嗎？"),
    KO("{0}이(가) 이미 있습니다. 바꿀까요?"),
    DE("{0} existiert bereits. Ersetzen?"),
    FR("{0} existe déjà. Le remplacer ?"),
    ES("{0} ya existe. ¿Reemplazarlo?"),
    PT("{0} já existe. Substituí-lo?"),
    IT("{0} esiste già. Sostituirlo?"),
    NL("{0} bestaat al. Vervangen?"),
    RU("{0} уже существует. Заменить?"),
    TR("{0} zaten var. Değiştirilsin mi?"));

SS_MSG(fd_replace_yes,
    EN("Replace"),
    JA("置き換える"),
    ZH_HANS("替换"),
    ZH_HANT("取代"),
    KO("바꾸기"),
    DE("Ersetzen"),
    FR("Remplacer"),
    ES("Reemplazar"),
    PT("Substituir"),
    IT("Sostituisci"),
    NL("Vervangen"),
    RU("Заменить"),
    TR("Değiştir"));


SS_MSG(seed_choice,
    EN("Starting points"), JA("初期点群"), ZH_HANS("初始点云"), ZH_HANT("初始點雲"), KO("시작 점 구름"),
    DE("Startpunkte"), FR("Points de départ"), ES("Puntos iniciales"), PT("Pontos iniciais"),
    IT("Punti iniziali"), NL("Startpunten"), RU("Начальные точки"), TR("Başlangıç noktaları"));
SS_MSG(seed_choice_dense,
    EN("Dense cloud"), JA("高密度点群"), ZH_HANS("稠密点云"), ZH_HANT("稠密點雲"), KO("고밀도 점 구름"),
    DE("Dichte Punktwolke"), FR("Nuage dense"), ES("Nube densa"), PT("Nuvem densa"), IT("Nuvola densa"),
    NL("Dichte puntenwolk"), RU("Плотное облако"), TR("Yoğun bulut"));
SS_MSG(seed_choice_sparse,
    EN("Sparse points"), JA("疎な点群"), ZH_HANS("稀疏点云"), ZH_HANT("稀疏點雲"), KO("희소 점 구름"),
    DE("Dünne Punktwolke"), FR("Points épars"), ES("Puntos dispersos"), PT("Pontos esparsos"), IT("Punti sparsi"),
    NL("IJle punten"), RU("Разреженные точки"), TR("Seyrek noktalar"));
SS_MSG(seed_choice_other,
    EN("Other PLY file"), JA("その他の PLY ファイル"), ZH_HANS("其他 PLY 文件"), ZH_HANT("其他 PLY 檔案"),
    KO("다른 PLY 파일"), DE("Andere PLY-Datei"), FR("Autre fichier PLY"), ES("Otro archivo PLY"),
    PT("Outro arquivo PLY"), IT("Altro file PLY"), NL("Ander PLY-bestand"), RU("Другой файл PLY"),
    TR("Başka bir PLY dosyası"));
SS_MSG(seed_choice_help,
    EN("The points training starts from. The dense cloud comes from dense reconstruction and usually gives finer detail sooner; the sparse points come from camera reconstruction."),
    JA("学習の出発点となる点群。高密度点群は高密度再構成の結果で、通常は細部がより早く出ます。疎な点群はカメラ推定で得られたものです。"),
    ZH_HANS("训练的起始点云。稠密点云来自稠密重建，通常能更快得到更细的细节；稀疏点云来自相机重建。"),
    ZH_HANT("訓練的起始點雲。稠密點雲來自稠密重建，通常能更快得到更細的細節；稀疏點雲來自相機重建。"),
    KO("학습을 시작할 점 구름입니다. 고밀도 점 구름은 고밀도 재구성 결과로 보통 세부가 더 빨리 살아나며, 희소 점 구름은 카메라 재구성에서 나옵니다."),
    DE("Die Punkte, mit denen das Training beginnt.\n- Die dichte Punktwolke stammt aus der dichten Rekonstruktion und liefert meist schneller feine Details;\n- die dünne aus der Kamerarekonstruktion."),
    FR("Les points de départ de l'entraînement.\n- Le nuage dense vient de la reconstruction dense et donne en général des détails fins plus tôt ;\n- les points épars viennent de la reconstruction des caméras."),
    ES("Los puntos desde los que empieza el entrenamiento.\n- La nube densa viene de la reconstrucción densa y suele dar detalle fino antes;\n- los puntos dispersos vienen de la reconstrucción de cámaras."),
    PT("Os pontos de onde o treinamento parte. A nuvem densa vem da reconstrução densa e costuma dar detalhes finos mais cedo; os pontos esparsos vêm da reconstrução das câmeras."),
    IT("I punti da cui parte l'addestramento.\n- La nuvola densa viene dalla ricostruzione densa e di solito dà dettagli fini prima;\n- i punti sparsi vengono dalla ricostruzione delle fotocamere."),
    NL("De punten waarmee de training begint.\n- De dichte puntenwolk komt uit de dichte reconstructie en geeft meestal sneller fijn detail;\n- de ijle punten komen uit de camerareconstructie."),
    RU("Точки, с которых начинается обучение. Плотное облако получено плотной реконструкцией и обычно быстрее даёт мелкие детали; разреженные точки — из реконструкции камер."),
    TR("Eğitimin başladığı noktalar.\n- Yoğun bulut yoğun yeniden yapılandırmadan gelir ve genellikle ince ayrıntıyı daha erken verir;\n- seyrek noktalar kamera yeniden yapılandırmasından gelir."));
SS_MSG(seed_source_random,
    EN("Point source: random initialization."), JA("点群の読み込み元：ランダム初期化。"),
    ZH_HANS("点云来源：随机初始化。"), ZH_HANT("點雲來源：隨機初始化。"), KO("점 구름 출처: 무작위 초기화."),
    DE("Punktquelle: zufällige Initialisierung."), FR("Source des points : initialisation aléatoire."),
    ES("Origen de puntos: inicialización aleatoria."), PT("Origem dos pontos: inicialização aleatória."),
    IT("Origine dei punti: inizializzazione casuale."), NL("Puntbron: willekeurige initialisatie."),
    RU("Источник точек: случайная инициализация."), TR("Nokta kaynağı: rastgele başlatma."));
SS_MSG(seed_source_resume,
    EN("Initialization: restore Gaussians from the checkpoint."), JA("初期化：チェックポイントのガウシアンを復元します。"),
    ZH_HANS("初始化来源：恢复检查点中的高斯。"), ZH_HANT("初始化來源：恢復檢查點中的高斯。"), KO("초기화: 체크포인트의 가우시안을 복원합니다."),
    DE("Initialisierung: Gaussians aus dem Checkpoint wiederherstellen."), FR("Initialisation : restaurer les gaussiennes du checkpoint."),
    ES("Inicialización: restaurar gaussianas del checkpoint."), PT("Inicialização: restaurar gaussianas do checkpoint."),
    IT("Inizializzazione: ripristina le gaussiane dal checkpoint."), NL("Initialisatie: Gaussians uit checkpoint herstellen."),
    RU("Инициализация: восстановление гауссиан из контрольной точки."), TR("Başlatma: kontrol noktasından Gaussianları geri yükle."));
SS_MSG(seed_source_splat,
    EN("Initialization: existing Gaussian PLY."), JA("初期化：既存のガウシアン PLY。"),
    ZH_HANS("初始化来源：已有高斯 PLY。"), ZH_HANT("初始化來源：已有高斯 PLY。"), KO("초기화: 기존 가우시안 PLY."),
    DE("Initialisierung: vorhandenes Gaussian-PLY."), FR("Initialisation : PLY gaussien existant."),
    ES("Inicialización: PLY gaussiano existente."), PT("Inicialização: PLY gaussiano existente."),
    IT("Inizializzazione: PLY gaussiano esistente."), NL("Initialisatie: bestaand Gaussian-PLY."),
    RU("Инициализация: существующий PLY гауссиан."), TR("Başlatma: mevcut Gaussian PLY."));
SS_MSG(seed_source_splat_add,
    EN("Initialization: existing Gaussian PLY plus point seeds."), JA("初期化：既存のガウシアン PLY に初期点群を追加します。"),
    ZH_HANS("初始化来源：已有高斯 PLY，同时追加点云。"), ZH_HANT("初始化來源：已有高斯 PLY，同時追加點雲。"), KO("초기화: 기존 가우시안 PLY에 초기 점 구름 추가."),
    DE("Initialisierung: vorhandenes Gaussian-PLY mit zusätzlichen Startpunkten."),
    FR("Initialisation : PLY gaussien existant et points initiaux supplémentaires."),
    ES("Inicialización: PLY gaussiano existente y puntos iniciales adicionales."),
    PT("Inicialização: PLY gaussiano existente e pontos iniciais adicionais."),
    IT("Inizializzazione: PLY gaussiano esistente e punti iniziali aggiuntivi."),
    NL("Initialisatie: bestaand Gaussian-PLY plus startpunten."),
    RU("Инициализация: существующий PLY гауссиан с добавлением начальных точек."), TR("Başlatma: mevcut Gaussian PLY ve ek başlangıç noktaları."));
SS_MSG(seed_cloud_unused,
    EN("The selected external cloud is not used for this initialization."), JA("選択した外部点群は今回の初期化には使われません。"),
    ZH_HANS("所选外部点云不参与本次初始化。"), ZH_HANT("所選外部點雲不參與本次初始化。"), KO("선택한 외부 점 구름은 이번 초기화에 사용되지 않습니다."),
    DE("Die gewählte externe Punktwolke wird für diese Initialisierung nicht verwendet."),
    FR("Le nuage externe sélectionné n'est pas utilisé pour cette initialisation."),
    ES("La nube externa seleccionada no se usa en esta inicialización."),
    PT("A nuvem externa selecionada não é usada nesta inicialização."),
    IT("La nuvola esterna selezionata non viene usata per questa inizializzazione."),
    NL("De geselecteerde externe puntenwolk wordt niet voor deze initialisatie gebruikt."),
    RU("Выбранное внешнее облако не используется для этой инициализации."), TR("Seçilen harici bulut bu başlatmada kullanılmaz."));

SS_MSG(resume_training,
    EN("Resume Training..."),
    JA("学習を再開…"),
    ZH_HANS("继续训练…"),
    ZH_HANT("繼續訓練…"),
    KO("학습 재개…"),
    DE("Training fortsetzen …"),
    FR("Reprendre l'entraînement…"),
    ES("Reanudar entrenamiento…"),
    PT("Retomar treinamento…"),
    IT("Riprendi addestramento…"),
    NL("Training hervatten…"),
    RU("Продолжить обучение…"),
    TR("Eğitime devam et…"));
SS_MSG(resume_training_help,
    EN("Open a run saved with Stop and Save, or a step checkpoint. Restores its dataset and settings; Start resumes toward the original step count."),
    JA("停止して保存した実行、またはステップのチェックポイントを開きます。データセットと設定を復元し、開始すると元の予定ステップ数まで続行します。"),
    ZH_HANS("打开“停止并保存”留下的运行或步骤检查点。恢复数据集和设置；点击开始，继续训练到原定步数。"),
    ZH_HANT("開啟「停止並儲存」留下的執行或步驟檢查點。還原資料集與設定；按下開始，繼續訓練至原定步數。"),
    KO("중지 후 저장한 실행이나 단계 체크포인트를 엽니다. 데이터셋과 설정을 복원하며, 시작하면 원래 목표 단계 수까지 이어서 학습합니다."),
    DE("Mit Stopp und Speichern gesicherten Lauf oder Schritt-Checkpoint öffnen. Lädt Daten und Einstellungen; Start setzt bis zur ursprünglichen Schrittzahl fort."),
    FR("Ouvrir une exécution arrêtée et sauvegardée, ou un point de contrôle. Restaure données et réglages ; Démarrer reprend vers le nombre initial d’étapes."),
    ES("Abre una ejecución guardada al detenerla o un punto de control. Restaura datos y ajustes; Iniciar continúa hasta el número de pasos original."),
    PT("Abra uma execução parada e guardada ou um checkpoint. Restaura dados e definições; Iniciar continua até ao número original de passos."),
    IT("Apri una sessione arrestata e salvata o un checkpoint. Ripristina dati e impostazioni; Avvia riprende fino al numero di passi originale."),
    NL("Open een met Stoppen en opslaan bewaarde run of een stapcheckpoint. Herstelt dataset en instellingen; Start hervat tot het oorspronkelijke aantal stappen."),
    RU("Откройте запуск после остановки с сохранением или контрольную точку шага. Восстановит данные и настройки; Старт продолжит до исходного числа шагов."),
    TR("Durdur ve Kaydet ile saklanan çalışmayı veya bir adım kontrol noktasını açın. Veriler ve ayarlar geri gelir; Başlat, ilk hedef adım sayısına kadar sürdürür."));
SS_MSG(resume_failed_title,
    EN("Cannot resume training"),
    JA("学習を再開できません"),
    ZH_HANS("无法继续训练"),
    ZH_HANT("無法繼續訓練"),
    KO("학습을 재개할 수 없습니다"),
    DE("Training kann nicht fortgesetzt werden"),
    FR("Impossible de reprendre l'entraînement"),
    ES("No se puede reanudar el entrenamiento"),
    PT("Não é possível retomar o treinamento"),
    IT("Impossibile riprendere l'addestramento"),
    NL("Training kan niet worden hervat"),
    RU("Не удаётся продолжить обучение"),
    TR("Eğitime devam edilemiyor"));
SS_MSG(resume_failed,
    EN("This folder cannot be resumed: {0}"),
    JA("このフォルダーは再開できません: {0}"),
    ZH_HANS("无法从此文件夹继续：{0}"),
    ZH_HANT("無法從此資料夾繼續：{0}"),
    KO("이 폴더에서 재개할 수 없습니다: {0}"),
    DE("Dieser Ordner kann nicht fortgesetzt werden: {0}"),
    FR("Ce dossier ne peut pas être repris : {0}"),
    ES("No se puede reanudar desde esta carpeta: {0}"),
    PT("Não é possível retomar a partir desta pasta: {0}"),
    IT("Impossibile riprendere da questa cartella: {0}"),
    NL("Deze map kan niet worden hervat: {0}"),
    RU("Невозможно продолжить из этой папки: {0}"),
    TR("Bu klasörden devam edilemiyor: {0}"));
SS_MSG(resume_failed_close,
    EN("OK"),
    JA("OK"),
    ZH_HANS("确定"),
    ZH_HANT("確定"),
    KO("확인"),
    DE("OK"),
    FR("OK"),
    ES("Aceptar"),
    PT("OK"),
    IT("OK"),
    NL("OK"),
    RU("ОК"),
    TR("Tamam"));

// ---- saved views ----

SS_MSG(viewport_views,
    EN("Views"),         JA("ビュー"),        ZH_HANS("视角"),
    ZH_HANT("視角"),      KO("뷰"),            DE("Ansichten"),
    FR("Vues"),          ES("Vistas"),       PT("Vistas"),
    IT("Viste"),         NL("Weergaven"),    RU("Виды"),
    TR("Görünümler"));

SS_MSG(view_default_name,
    EN("View {0}"),      JA("ビュー {0}"),    ZH_HANS("视角 {0}"),
    ZH_HANT("視角 {0}"),  KO("뷰 {0}"),        DE("Ansicht {0}"),
    FR("Vue {0}"),       ES("Vista {0}"),    PT("Vista {0}"),
    IT("Vista {0}"),     NL("Weergave {0}"), RU("Вид {0}"),
    TR("Görünüm {0}"));

SS_MSG(view_orthographic,
    EN("Orthographic"),  JA("平行投影"),      ZH_HANS("正交"),
    ZH_HANT("正交"),      KO("직교"),          DE("Orthografisch"),
    FR("Orthographique"), ES("Ortográfica"), PT("Ortográfica"),
    IT("Ortografica"),   NL("Orthografisch"), RU("Ортографическая"),
    TR("Ortografik"));

SS_MSG(view_slot_help,
    EN("{0}: {1}, {2}. Saved {3}. Click or {4} goes there, {5} saves the "
       "current view over it; right-click for more."),
    JA("{0}: {1}、{2}。保存日時 {3}。クリックまたは {4} で移動、{5} で現在の"
       "ビューを上書き保存します。右クリックでその他の操作。"),
    ZH_HANS("{0}：{1}，{2}。保存于 {3}。单击或按 {4} 前往，按 {5} 用当前视角"
            "覆盖保存；右键单击查看更多。"),
    ZH_HANT("{0}：{1}，{2}。儲存於 {3}。按一下或按 {4} 前往，按 {5} 以目前視角"
            "覆蓋儲存；按右鍵查看更多。"),
    KO("{0}: {1}, {2}. 저장: {3}. 클릭하거나 {4}를 누르면 이동하고, {5}는 현재 "
       "뷰로 덮어씁니다. 더 많은 작업은 오른쪽 클릭."),
    DE("{0}: {1}, {2}. Gespeichert {3}. Klick oder {4} springt dorthin, {5} "
       "überschreibt sie mit der aktuellen Ansicht; Rechtsklick für mehr."),
    FR("{0} : {1}, {2}. Enregistrée le {3}. Un clic ou {4} y va, {5} la "
       "remplace par la vue actuelle ; clic droit pour plus."),
    ES("{0}: {1}, {2}. Guardada el {3}. Un clic o {4} va allí, {5} la "
       "sustituye por la vista actual; clic derecho para más."),
    PT("{0}: {1}, {2}. Salva em {3}. Um clique ou {4} vai até ela, {5} a "
       "substitui pela vista atual; clique direito para mais."),
    IT("{0}: {1}, {2}. Salvata il {3}. Un clic o {4} la raggiunge, {5} la "
       "sostituisce con la vista attuale; clic destro per altro."),
    NL("{0}: {1}, {2}. Opgeslagen {3}. Klik of {4} gaat erheen, {5} "
       "overschrijft hem met de huidige weergave; rechtsklik voor meer."),
    RU("{0}: {1}, {2}. Сохранён {3}. Щелчок или {4} переходит к нему, {5} "
       "заменяет его текущим видом; правый щелчок — другие действия."),
    TR("{0}: {1}, {2}. Kaydedildi: {3}. Tıklama veya {4} oraya gider, {5} "
       "mevcut görünümle üzerine yazar; daha fazlası için sağ tıklayın."));

SS_MSG(view_empty_help,
    EN("Empty. Click or {0} saves the current view here."),
    JA("空きスロット。クリックまたは {0} で現在のビューをここに保存します。"),
    ZH_HANS("空位。单击或按 {0} 将当前视角保存到这里。"),
    ZH_HANT("空位。按一下或按 {0} 將目前視角儲存到這裡。"),
    KO("비어 있음. 클릭하거나 {0}를 누르면 현재 뷰를 여기에 저장합니다."),
    DE("Leer. Klick oder {0} speichert die aktuelle Ansicht hier."),
    FR("Vide. Un clic ou {0} enregistre ici la vue actuelle."),
    ES("Vacía. Un clic o {0} guarda aquí la vista actual."),
    PT("Vazia. Um clique ou {0} salva aqui a vista atual."),
    IT("Vuota. Un clic o {0} salva qui la vista attuale."),
    NL("Leeg. Klik of {0} slaat de huidige weergave hier op."),
    RU("Пусто. Щелчок или {0} сохраняет сюда текущий вид."),
    TR("Boş. Tıklama veya {0} mevcut görünümü buraya kaydeder."));

SS_MSG(view_save_here,
    EN("Save current view here"),       JA("現在のビューをここに保存"),
    ZH_HANS("将当前视角保存到这里"),      ZH_HANT("將目前視角儲存到這裡"),
    KO("현재 뷰를 여기에 저장"),          DE("Aktuelle Ansicht hier speichern"),
    FR("Enregistrer la vue actuelle ici"), ES("Guardar aquí la vista actual"),
    PT("Salvar aqui a vista atual"),    IT("Salva qui la vista attuale"),
    NL("Huidige weergave hier opslaan"), RU("Сохранить сюда текущий вид"),
    TR("Mevcut görünümü buraya kaydet"));

SS_MSG(view_rename,
    EN("Rename"),        JA("名前を変更"),    ZH_HANS("重命名"),
    ZH_HANT("重新命名"),  KO("이름 바꾸기"),   DE("Umbenennen"),
    FR("Renommer"),      ES("Renombrar"),    PT("Renomear"),
    IT("Rinomina"),      NL("Hernoemen"),    RU("Переименовать"),
    TR("Yeniden adlandır"));

SS_MSG(view_clear,
    EN("Clear"),         JA("消去"),          ZH_HANS("清除"),
    ZH_HANT("清除"),      KO("지우기"),        DE("Löschen"),
    FR("Effacer"),       ES("Borrar"),       PT("Limpar"),
    IT("Cancella"),      NL("Wissen"),       RU("Очистить"),
    TR("Temizle"));

SS_MSG(view_save_failed,
    EN("Could not save the views: {0}"),
    JA("ビューを保存できませんでした: {0}"),
    ZH_HANS("无法保存视角：{0}"),
    ZH_HANT("無法儲存視角：{0}"),
    KO("뷰를 저장할 수 없습니다: {0}"),
    DE("Ansichten konnten nicht gespeichert werden: {0}"),
    FR("Impossible d'enregistrer les vues : {0}"),
    ES("No se pudieron guardar las vistas: {0}"),
    PT("Não foi possível salvar as vistas: {0}"),
    IT("Impossibile salvare le viste: {0}"),
    NL("Kan de weergaven niet opslaan: {0}"),
    RU("Не удалось сохранить виды: {0}"),
    TR("Görünümler kaydedilemedi: {0}"));

// ---- screenshots ----

SS_MSG(screenshot,
    EN("Screenshot"),    JA("スクリーンショット"), ZH_HANS("截图"),
    ZH_HANT("截圖"),      KO("스크린샷"),       DE("Screenshot"),
    FR("Capture"),       ES("Captura"),      PT("Captura"),
    IT("Screenshot"),    NL("Schermafbeelding"), RU("Снимок"),
    TR("Ekran görüntüsü"));

SS_MSG(screenshot_help,
    EN("Render the current view at {0} x {1} and save it in {2}. Training "
       "keeps running."),
    JA("現在のビューを {0} x {1} でレンダリングし、{2} に保存します。学習は"
       "そのまま続きます。"),
    ZH_HANS("以 {0} x {1} 渲染当前视角并保存到 {2}。训练不会中断。"),
    ZH_HANT("以 {0} x {1} 算繪目前視角並儲存到 {2}。訓練不會中斷。"),
    KO("현재 뷰를 {0} x {1}로 렌더링해 {2}에 저장합니다. 학습은 계속됩니다."),
    DE("Rendert die aktuelle Ansicht mit {0} x {1} und speichert sie in {2}. "
       "Das Training läuft weiter."),
    FR("Rend la vue actuelle en {0} x {1} et l'enregistre dans {2}. "
       "L'entraînement continue."),
    ES("Renderiza la vista actual a {0} x {1} y la guarda en {2}. El "
       "entrenamiento sigue."),
    PT("Renderiza a vista atual em {0} x {1} e a salva em {2}. O treino "
       "continua."),
    IT("Esegue il rendering della vista attuale a {0} x {1} e la salva in {2}. "
       "L'addestramento continua."),
    NL("Rendert de huidige weergave op {0} x {1} en slaat die op in {2}. De "
       "training loopt door."),
    RU("Рендерит текущий вид в {0} x {1} и сохраняет в {2}. Обучение "
       "продолжается."),
    TR("Mevcut görünümü {0} x {1} olarak işler ve {2} içine kaydeder. Eğitim "
       "sürer."));

SS_MSG(screenshot_unavailable,
    EN("Available in the 3D view once training has started."),
    JA("学習を開始すると 3D ビューで使えます。"),
    ZH_HANS("训练开始后可在 3D 视图中使用。"),
    ZH_HANT("訓練開始後可在 3D 檢視中使用。"),
    KO("학습을 시작하면 3D 뷰에서 쓸 수 있습니다."),
    DE("Verfügbar in der 3D-Ansicht, sobald das Training läuft."),
    FR("Disponible dans la vue 3D une fois l'entraînement lancé."),
    ES("Disponible en la vista 3D cuando el entrenamiento ha empezado."),
    PT("Disponível na vista 3D depois que o treino começa."),
    IT("Disponibile nella vista 3D una volta avviato l'addestramento."),
    NL("Beschikbaar in de 3D-weergave zodra de training loopt."),
    RU("Доступно в 3D-виде после начала обучения."),
    TR("Eğitim başladıktan sonra 3B görünümde kullanılabilir."));

SS_MSG(screenshot_settings,
    EN("Screenshot settings"),          JA("スクリーンショットの設定"),
    ZH_HANS("截图设置"),                  ZH_HANT("截圖設定"),
    KO("스크린샷 설정"),                   DE("Screenshot-Einstellungen"),
    FR("Réglages de capture"),          ES("Ajustes de captura"),
    PT("Configurações de captura"),     IT("Impostazioni screenshot"),
    NL("Instellingen schermafbeelding"), RU("Настройки снимка"),
    TR("Ekran görüntüsü ayarları"));

SS_MSG(screenshot_folder,
    EN("Folder"),        JA("フォルダー"),    ZH_HANS("文件夹"),
    ZH_HANT("資料夾"),    KO("폴더"),          DE("Ordner"),
    FR("Dossier"),       ES("Carpeta"),      PT("Pasta"),
    IT("Cartella"),      NL("Map"),          RU("Папка"),
    TR("Klasör"));

SS_MSG(screenshot_folder_help,
    EN("A relative folder starts in the dataset folder. Wildcards: {dataset}, "
       "{run}."),
    JA("相対パスはデータセットのフォルダーが起点です。ワイルドカード: "
       "{dataset}、{run}。"),
    ZH_HANS("相对路径从数据集文件夹开始。通配符：{dataset}、{run}。"),
    ZH_HANT("相對路徑從資料集資料夾開始。萬用字元：{dataset}、{run}。"),
    KO("상대 경로는 데이터셋 폴더에서 시작합니다. 와일드카드: {dataset}, {run}."),
    DE("Ein relativer Ordner beginnt im Datensatzordner. Platzhalter: "
       "{dataset}, {run}."),
    FR("Un dossier relatif part du dossier du jeu de données. Jokers : "
       "{dataset}, {run}."),
    ES("Una carpeta relativa parte de la carpeta del conjunto de datos. "
       "Comodines: {dataset}, {run}."),
    PT("Uma pasta relativa parte da pasta do conjunto de dados. Curingas: "
       "{dataset}, {run}."),
    IT("Una cartella relativa parte dalla cartella del dataset. Segnaposto: "
       "{dataset}, {run}."),
    NL("Een relatieve map begint in de datasetmap. Jokertekens: {dataset}, "
       "{run}."),
    RU("Относительная папка отсчитывается от папки набора данных. Шаблоны: "
       "{dataset}, {run}."),
    TR("Göreli bir klasör veri kümesi klasöründen başlar. Joker karakterler: "
       "{dataset}, {run}."));

SS_MSG(screenshot_name,
    EN("File name"),     JA("ファイル名"),    ZH_HANS("文件名"),
    ZH_HANT("檔名"),      KO("파일 이름"),     DE("Dateiname"),
    FR("Nom du fichier"), ES("Nombre de archivo"), PT("Nome do arquivo"),
    IT("Nome file"),     NL("Bestandsnaam"), RU("Имя файла"),
    TR("Dosya adı"));

SS_MSG(screenshot_name_help,
    EN("Wildcards: {run} run folder, {view} saved view the camera is on (left out when there is none), {n} running number, {step} training step, {dataset} dataset folder, {date}, {time}."),
    JA("ワイルドカード: {run} 実行フォルダー、{view} カメラがいる保存ビュー（なければ省略）、{n} 連番、{step} 学習ステップ、{dataset} データセットのフォルダー、{date}、{time}。"),
    ZH_HANS("通配符：{run} 运行文件夹，{view} 相机所在的已存视角（没有则省略），{n} 序号，{step} 训练步数，{dataset} 数据集文件夹，{date}，{time}。"),
    ZH_HANT("萬用字元：{run} 執行資料夾，{view} 相機所在的已存視角（沒有則省略），{n} 序號，{step} 訓練步數，{dataset} 資料集資料夾，{date}，{time}。"),
    KO("와일드카드: {run} 실행 폴더, {view} 카메라가 있는 저장된 뷰(없으면 생략), {n} 일련번호, {step} 학습 단계, {dataset} 데이터셋 폴더, {date}, {time}."),
    DE("- {run}: Lauf-Ordner; {dataset}: Datensatzordner.\n- {view}: aktuelle gespeicherte Ansicht, ohne Ansicht weggelassen.\n- {n}: laufende Nummer; {step}: Trainingsschritt; {date}, {time}: Datum, Uhrzeit."),
    FR("- {run} : dossier d’exécution ; {dataset} : dossier de données.\n- {view} : vue enregistrée actuelle, omise sans vue.\n- {n} : numéro ; {step} : étape ; {date}, {time} : date, heure."),
    ES("- {run}: carpeta de ejecución; {dataset}: carpeta de datos.\n- {view}: vista guardada actual, se omite si no hay.\n- {n}: número correlativo; {step}: paso; {date}, {time}: fecha, hora."),
    PT("- {run}: pasta de execução; {dataset}: pasta de dados.\n- {view}: vista guardada atual, omitida se não houver.\n- {n}: número sequencial; {step}: passo; {date}, {time}: data, hora."),
    IT("- {run}: cartella di esecuzione; {dataset}: cartella dei dati.\n- {view}: vista salvata attuale, omessa se assente.\n- {n}: numero progressivo; {step}: passo; {date}, {time}: data, ora."),
    NL("- {run}: run-map; {dataset}: datasetmap.\n- {view}: huidige opgeslagen weergave, weggelaten als die ontbreekt.\n- {n}: volgnummer; {step}: trainingsstap; {date}, {time}: datum, tijd."),
    RU("- {run}: папка запуска; {dataset}: папка данных.\n- {view}: текущий сохранённый вид, опускается без него.\n- {n}: порядковый номер; {step}: шаг; {date}, {time}: дата, время."),
    TR("- {run}: çalışma klasörü; {dataset}: veri klasörü.\n- {view}: mevcut kayıtlı görünüm, yoksa atlanır.\n- {n}: sıra numarası; {step}: adım; {date}, {time}: tarih, saat."));

SS_MSG(screenshot_format,
    EN("Format"),        JA("形式"),          ZH_HANS("格式"),
    ZH_HANT("格式"),      KO("형식"),          DE("Format"),
    FR("Format"),        ES("Formato"),      PT("Formato"),
    IT("Formato"),       NL("Formaat"),      RU("Формат"),
    TR("Biçim"));

SS_MSG(screenshot_quality,
    EN("JPG quality"),   JA("JPG 品質"),      ZH_HANS("JPG 质量"),
    ZH_HANT("JPG 品質"),  KO("JPG 품질"),      DE("JPG-Qualität"),
    FR("Qualité JPG"),   ES("Calidad JPG"),  PT("Qualidade JPG"),
    IT("Qualità JPG"),   NL("JPG-kwaliteit"), RU("Качество JPG"),
    TR("JPG kalitesi"));

SS_MSG(screenshot_size,
    EN("Size in pixels"), JA("サイズ（ピクセル）"), ZH_HANS("尺寸（像素）"),
    ZH_HANT("尺寸（像素）"), KO("크기(픽셀)"),   DE("Größe in Pixeln"),
    FR("Taille en pixels"), ES("Tamaño en píxeles"), PT("Tamanho em pixels"),
    IT("Dimensione in pixel"), NL("Grootte in pixels"), RU("Размер в пикселях"),
    TR("Piksel cinsinden boyut"));

SS_MSG(screenshot_overlay,
    EN("Info overlay"),  JA("情報オーバーレイ"), ZH_HANS("信息叠加"),
    ZH_HANT("資訊疊加"),  KO("정보 오버레이"), DE("Info-Einblendung"),
    FR("Encart d'infos"), ES("Recuadro de datos"), PT("Quadro de dados"),
    IT("Riquadro info"), NL("Info-overlay"), RU("Инфо-плашка"),
    TR("Bilgi kutusu"));

SS_MSG(screenshot_overlay_help,
    EN("Steps, splats and VRAM in the bottom-right corner of the image."),
    JA("画像の右下にステップ数、スプラット数、VRAM を表示します。"),
    ZH_HANS("在图像右下角显示步数、泼溅数和显存。"),
    ZH_HANT("在影像右下角顯示步數、潑濺數和顯示記憶體。"),
    KO("이미지 오른쪽 아래에 단계, 스플랫, VRAM을 표시합니다."),
    DE("Schritte, Splats und VRAM in der rechten unteren Bildecke."),
    FR("Étapes, splats et VRAM dans le coin inférieur droit de l'image."),
    ES("Pasos, splats y VRAM en la esquina inferior derecha de la imagen."),
    PT("Passos, splats e VRAM no canto inferior direito da imagem."),
    IT("Passi, splat e VRAM nell'angolo in basso a destra dell'immagine."),
    NL("Stappen, splats en VRAM in de rechteronderhoek van het beeld."),
    RU("Шаги, сплаты и видеопамять в правом нижнем углу изображения."),
    TR("Görüntünün sağ alt köşesinde adımlar, splat'ler ve VRAM."));

SS_MSG(screenshot_defaults,
    EN("Reset to defaults"),            JA("既定に戻す"),
    ZH_HANS("恢复默认值"),                ZH_HANT("還原預設值"),
    KO("기본값으로 재설정"),               DE("Auf Standard zurücksetzen"),
    FR("Valeurs par défaut"),           ES("Restablecer valores"),
    PT("Restaurar padrões"),            IT("Ripristina predefiniti"),
    NL("Standaard herstellen"),         RU("Сбросить по умолчанию"),
    TR("Varsayılanlara dön"));

SS_MSG(screenshot_saved,
    EN("Screenshot saved: {0}"),        JA("スクリーンショットを保存しました: {0}"),
    ZH_HANS("截图已保存：{0}"),           ZH_HANT("截圖已儲存：{0}"),
    KO("스크린샷 저장됨: {0}"),            DE("Screenshot gespeichert: {0}"),
    FR("Capture enregistrée : {0}"),    ES("Captura guardada: {0}"),
    PT("Captura salva: {0}"),           IT("Screenshot salvato: {0}"),
    NL("Schermafbeelding opgeslagen: {0}"), RU("Снимок сохранён: {0}"),
    TR("Ekran görüntüsü kaydedildi: {0}"));

SS_MSG(screenshot_failed,
    EN("Screenshot failed: {0}"),       JA("スクリーンショットに失敗しました: {0}"),
    ZH_HANS("截图失败：{0}"),             ZH_HANT("截圖失敗：{0}"),
    KO("스크린샷 실패: {0}"),              DE("Screenshot fehlgeschlagen: {0}"),
    FR("Échec de la capture : {0}"),    ES("Error en la captura: {0}"),
    PT("Falha na captura: {0}"),        IT("Screenshot non riuscito: {0}"),
    NL("Schermafbeelding mislukt: {0}"), RU("Не удалось сделать снимок: {0}"),
    TR("Ekran görüntüsü alınamadı: {0}"));

SS_MSG(screenshot_viewer_help,
    EN("Render every open splat model at {0} x {1} from the current view and save each in its own dataset's screenshot folder. Step and splat cap come from the run folder the file is in."),
    JA("現在のビューから全モデルを {0} x {1} で描画し、各データセットのスクリーンショットフォルダに保存。\nステップとスプラット上限は、ファイルのある実行フォルダから取得します。"),
    ZH_HANS("以 {0} x {1} 从当前视角渲染每个打开的泼溅模型，并分别保存到各自数据集的截图文件夹。步数和泼溅上限取自文件所在的运行文件夹。"),
    ZH_HANT("以 {0} x {1} 從目前視角算繪每個開啟的潑濺模型，並分別儲存到各自資料集的截圖資料夾。步數和潑濺上限取自檔案所在的執行資料夾。"),
    KO("열려 있는 모든 스플랫 모델을 현재 뷰에서 {0} x {1}로 렌더링해 각 데이터셋의 스크린샷 폴더에 저장합니다. 단계와 스플랫 상한은 파일이 있는 실행 폴더에서 가져옵니다."),
    DE("Rendert alle offenen Splats mit {0} x {1} aus dieser Ansicht in den Screenshot-Ordner ihres Datensatzes.\nSchritt und Splat-Limit stammen aus dem Lauf-Ordner der Datei."),
    FR("Rend les modèles ouverts en {0} x {1} dans le dossier de captures de chaque jeu de données.\nVue actuelle ; étape et limite de splats du dossier d’exécution du fichier."),
    ES("Renderiza los modelos abiertos a {0} x {1} en la carpeta de capturas de sus datos.\nVista actual; paso y límite de splats de la carpeta de ejecución del archivo."),
    PT("Renderiza os modelos abertos a {0} x {1} na pasta de capturas dos seus dados.\nVista atual; passo e limite de splats da pasta de execução do ficheiro."),
    IT("Renderizza i modelli aperti a {0} x {1} nella cartella screenshot dei rispettivi dati.\nVista attuale; passo e limite di splat dalla cartella di esecuzione del file."),
    NL("Rendert open modellen op {0} x {1} in de screenshotmap van hun dataset.\nHuidige weergave; stap en splat-limiet uit de run-map van het bestand."),
    RU("Сохраняет открытые модели в {0} x {1} из текущего вида в папку снимков каждого набора данных.\nШаг и лимит сплатов берутся из папки запуска файла."),
    TR("Açık modelleri mevcut görünümden {0} x {1} olarak veri kümelerinin ekran görüntüsü klasörüne kaydeder.\nAdım ve splat sınırı dosyanın çalışma klasöründen alınır."));

SS_MSG(screenshot_viewer_unavailable,
    EN("Open a splat model first."),    JA("先にスプラットモデルを開いてください。"),
    ZH_HANS("请先打开一个泼溅模型。"),      ZH_HANT("請先開啟一個潑濺模型。"),
    KO("먼저 스플랫 모델을 여세요."),       DE("Zuerst ein Splat-Modell öffnen."),
    FR("Ouvrez d'abord un modèle de splats."), ES("Abre primero un modelo de splats."),
    PT("Abra primeiro um modelo de splats."), IT("Apri prima un modello di splat."),
    NL("Open eerst een splat-model."),  RU("Сначала откройте модель сплатов."),
    TR("Önce bir splat modeli açın."));

}  // namespace gui
}  // namespace msg
}  // namespace i18n
}  // namespace spirula

#include "i18n/EndCatalog.h"
