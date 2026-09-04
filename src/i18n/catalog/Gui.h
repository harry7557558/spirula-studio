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
    EN("Pick files with the desktop's own file browser. Turn this off to use "
       "the simple built-in one, which is the only option on a session where "
       "no system dialog can be reached."),
    JA("ファイル選択にデスクトップ標準のファイルブラウザを使います。オフにすると"
       "内蔵の簡易ブラウザを使います。システムの選択画面を開けない環境では、"
       "そちらだけが使えます。"),
    ZH_HANS("用桌面自带的文件浏览器来选择文件。关闭后使用内置的简易浏览器；在无法"
            "调用系统对话框的环境里，内置的是唯一选择。"),
    ZH_HANT("用桌面自帶的檔案瀏覽器來選擇檔案。關閉後使用內建的簡易瀏覽器；在無法"
            "呼叫系統對話框的環境裡，內建的是唯一選擇。"),
    KO("바탕화면 환경의 파일 탐색기로 파일을 고릅니다. 끄면 내장된 간단한 "
       "탐색기를 씁니다. 시스템 대화상자를 열 수 없는 환경에서는 내장 쪽만 "
       "쓸 수 있습니다."),
    DE("Dateien mit dem Dateibrowser der Arbeitsumgebung auswählen. "
       "Ausgeschaltet wird der einfache eingebaute verwendet, der auf einer "
       "Sitzung ohne erreichbaren Systemdialog die einzige Möglichkeit ist."),
    FR("Choisir les fichiers avec le navigateur du bureau. Désactivez pour "
       "utiliser celui, plus simple, intégré au programme, qui est le seul "
       "possible sur une session sans dialogue système accessible."),
    ES("Elegir archivos con el navegador del escritorio. Desactívelo para usar "
       "el sencillo navegador integrado, que es la única opción en una sesión "
       "donde no hay ningún diálogo del sistema accesible."),
    PT("Escolher arquivos com o navegador do próprio ambiente de trabalho. "
       "Desative para usar o navegador interno mais simples, que é a única "
       "opção numa sessão sem nenhuma caixa de diálogo do sistema acessível."),
    IT("Scegli i file con il browser dell'ambiente desktop. Disattivalo per "
       "usare quello semplice incorporato, che è l'unica possibilità in una "
       "sessione dove nessuna finestra di sistema è raggiungibile."),
    NL("Bestanden kiezen met de bestandsbeheerder van de werkomgeving. Zet dit "
       "uit voor de eenvoudige ingebouwde versie, die de enige mogelijkheid is "
       "in een sessie zonder bereikbaar systeemvenster."),
    RU("Выбирать файлы через файловый менеджер рабочего стола. Выключите, "
       "чтобы пользоваться простым встроенным, — в сеансе, где системный "
       "диалог недоступен, он единственный возможный."),
    TR("Dosyaları masaüstünün kendi dosya tarayıcısıyla seçin. Kapatınca "
       "yerleşik basit tarayıcı kullanılır; sistem penceresine erişilemeyen "
       "bir oturumda tek seçenek odur."));

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
    EN("Photos of a scene or object, video clips walking around one, or "
       "several of both in one capture. Each input gets its own camera, the "
       "camera positions are worked out for you, and the result opens "
       "straight in the trainer."),
    JA("シーンや被写体の写真、そのまわりを歩いて撮った動画、あるいはその両方を"
       "まとめて選べます。入力ごとに1台のカメラになり、カメラ位置は自動で"
       "求められ、結果はそのままトレーナーで開きます。"),
    ZH_HANS("场景或物体的照片、绕着它行走拍摄的视频，或者两者一起。每个输入"
            "各自作为一台相机，相机位置会自动求解，结果直接在训练器中打开。"),
    ZH_HANT("場景或物體的相片、繞著它行走拍攝的影片，或者兩者一起。每個輸入"
            "各自作為一台相機，相機位置會自動求解，結果直接在訓練器中開啟。"),
    KO("장면이나 물체의 사진, 그 주위를 걸으며 찍은 동영상, 또는 둘 다를 한 "
       "번에 고를 수 있습니다. 입력마다 하나의 카메라가 되고, 카메라 위치는 "
       "자동으로 계산되며 결과는 곧바로 트레이너에서 열립니다."),
    DE("Fotos einer Szene oder eines Objekts, Videos, die darum herumführen, "
       "oder beides in einer Aufnahme. Jede Eingabe bekommt ihre eigene "
       "Kamera, die Kamerapositionen werden für Sie berechnet, und das "
       "Ergebnis öffnet sich direkt im Trainer."),
    FR("Des photos d'une scène ou d'un objet, des vidéos qui en font le tour, "
       "ou les deux dans une même prise. Chaque entrée reçoit sa propre "
       "caméra, les positions de caméra sont calculées pour vous et le "
       "résultat s'ouvre directement dans l'atelier."),
    ES("Fotos de una escena u objeto, vídeos que lo recorren, o ambos en una "
       "misma captura. Cada entrada recibe su propia cámara, las posiciones "
       "de cámara se calculan automáticamente y el resultado se abre "
       "directamente en el entrenador."),
    PT("Fotos de uma cena ou objeto, vídeos que percorrem a cena, ou os dois "
       "na mesma captura. Cada entrada recebe a sua própria câmera, as "
       "posições de câmera são calculadas para você e o resultado abre direto "
       "no treinador."),
    IT("Fotografie di una scena o di un oggetto, video che vi girano attorno, "
       "oppure entrambi nella stessa ripresa. Ogni ingresso riceve la propria "
       "fotocamera, le posizioni della fotocamera vengono calcolate "
       "automaticamente e il risultato si apre direttamente nell'addestratore."),
    NL("Foto's van een scène of object, video's die eromheen lopen, of allebei "
       "in één opname. Elke invoer krijgt een eigen camera, de cameraposities "
       "worden voor u berekend en het resultaat opent meteen in de trainer."),
    RU("Фотографии сцены или объекта, видео с обходом вокруг них, или и то и "
       "другое в одной съёмке. Каждый вход получает свою камеру, положения "
       "камер вычисляются автоматически, а результат сразу откроется в "
       "тренажёре."),
    TR("Bir sahnenin veya nesnenin fotoğrafları, çevresinde dolaşan videolar "
       "veya ikisi birden aynı çekimde. Her girdi kendi kamerasını alır, "
       "kamera konumları sizin için hesaplanır ve sonuç doğrudan eğiticide "
       "açılır."));

SS_MSG(home_drop_hint,
    EN("...or drop a dataset folder, photo folders, video files, or a model or "
       "mesh file anywhere in this window"),
    JA("…または、データセットフォルダ・写真フォルダ・動画ファイル・モデルや"
       "メッシュのファイルをこのウィンドウのどこかにドロップしてください"),
    ZH_HANS("…或者把数据集文件夹、照片文件夹、视频文件，或者模型和网格文件拖到"
            "这个窗口的任意位置"),
    ZH_HANT("…或者把資料集資料夾、相片資料夾、影片檔，或者模型和網格檔案拖到"
            "這個視窗的任意位置"),
    KO("…또는 데이터셋 폴더, 사진 폴더, 동영상 파일, 모델이나 메시 파일을 이 창 "
       "아무 곳에나 끌어다 놓으세요"),
    DE("… oder ziehen Sie einen Datensatzordner, Fotoordner, Videodateien oder "
       "eine Modell- oder Netzdatei irgendwo in dieses Fenster"),
    FR("… ou déposez un dossier de jeu de données, des dossiers de photos, des "
       "fichiers vidéo, ou un fichier de modèle ou de maillage n'importe où "
       "dans cette fenêtre"),
    ES("… o arrastre una carpeta de conjunto de datos, carpetas de fotos, "
       "archivos de vídeo, o un archivo de modelo o de malla a cualquier punto "
       "de esta ventana"),
    PT("… ou arraste uma pasta de conjunto de dados, pastas de fotos, arquivos "
       "de vídeo, ou um arquivo de modelo ou de malha para qualquer ponto "
       "desta janela"),
    IT("… oppure trascina una cartella di dataset, cartelle di foto, file video, "
       "o un file di modello o di mesh in un punto qualsiasi di questa "
       "finestra"),
    NL("… of sleep een datasetmap, fotomappen, videobestanden, of een model- of "
       "meshbestand ergens in dit venster"),
    RU("…или перетащите папку набора данных, папки с фотографиями, видеофайлы, "
       "либо файл модели или меша в любое место этого окна"),
    TR("…ya da bir veri kümesi klasörünü, fotoğraf klasörlerini, video "
       "dosyalarını veya bir model ya da ağ dosyasını bu pencerenin herhangi "
       "bir yerine bırakın"));

SS_MSG(home_recent,
    EN("Recent"),        JA("最近使った項目"), ZH_HANS("最近"),   ZH_HANT("最近"),
    KO("최근 항목"),      DE("Zuletzt"),      FR("Récents"),     ES("Recientes"),
    PT("Recentes"),      IT("Recenti"),      NL("Recent"),      RU("Недавние"),
    TR("Son kullanılan"));

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

SS_MSG(device_locked,
    EN("Device is fixed once training starts; restart the app to change it."),
    JA("学習を始めるとデバイスは固定されます。変更するにはアプリを再起動して"
       "ください。"),
    ZH_HANS("训练开始后设备就固定了；要更换请重新启动程序。"),
    ZH_HANT("訓練開始後裝置就固定了；要更換請重新啟動程式。"),
    KO("학습이 시작되면 장치가 고정됩니다. 바꾸려면 앱을 다시 시작하세요."),
    DE("Das Gerät steht fest, sobald das Training beginnt; zum Wechseln die "
       "Anwendung neu starten."),
    FR("Le périphérique est figé une fois l'entraînement lancé ; redémarrez "
       "l'application pour en changer."),
    ES("El dispositivo queda fijado en cuanto empieza el entrenamiento; "
       "reinicie la aplicación para cambiarlo."),
    PT("O dispositivo fica fixo assim que o treinamento começa; reinicie o "
       "aplicativo para trocá-lo."),
    IT("Il dispositivo è fissato una volta avviato l'addestramento; riavvii "
       "l'applicazione per cambiarlo."),
    NL("Het apparaat ligt vast zodra de training begint; herstart de "
       "toepassing om het te wijzigen."),
    RU("После начала обучения устройство менять нельзя; чтобы выбрать другое, "
       "перезапустите программу."),
    TR("Eğitim başladıktan sonra aygıt sabitlenir; değiştirmek için "
       "uygulamayı yeniden başlatın."));

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
    EN("Upper bound on the number of Gaussians. More captures more detail but "
       "uses more VRAM and renders slower. ~1M suits most small scenes; large "
       "or highly detailed scenes may need more."),
    JA("ガウシアンの個数の上限です。多いほど細部を捉えられますが、VRAM を"
       "多く使い描画も遅くなります。小さめのシーンはたいてい 100 万程度で"
       "足り、広いシーンや細部の多いシーンではもっと必要になることがあります。"),
    ZH_HANS("高斯基元数量的上限。数量越多细节越丰富，但显存占用更大、渲染更慢。"
            "约 100 万适合大多数小场景；大场景或细节很多的场景可能需要更多。"),
    ZH_HANT("高斯基元數量的上限。數量越多細節越豐富，但顯示記憶體佔用更大、算繪更慢。"
            "約 100 萬適合大多數小場景；大場景或細節很多的場景可能需要更多。"),
    KO("가우시안 개수의 상한입니다. 많을수록 디테일이 살아나지만 VRAM을 더 "
       "쓰고 렌더링이 느려집니다. 작은 장면은 대개 100만이면 충분하고, 넓거나 "
       "세부가 많은 장면은 더 필요할 수 있습니다."),
    DE("Obergrenze für die Zahl der Gaußfunktionen. Mehr erfasst mehr Details, "
       "braucht aber mehr VRAM und rendert langsamer. Rund 1 Mio. passt für "
       "die meisten kleinen Szenen; große oder sehr detailreiche brauchen "
       "mehr."),
    FR("Borne supérieure du nombre de gaussiennes. Davantage capture plus de "
       "détails, mais consomme plus de VRAM et rend plus lentement. Environ "
       "1 M convient à la plupart des petites scènes ; les grandes ou très "
       "détaillées en demandent davantage."),
    ES("Límite superior del número de gaussianas. Más captura más detalle, "
       "pero usa más VRAM y renderiza más lento. Alrededor de 1 M vale para "
       "casi todas las escenas pequeñas; las grandes o muy detalladas piden "
       "más."),
    PT("Limite superior do número de gaussianas. Mais captura mais detalhe, "
       "mas usa mais VRAM e renderiza mais devagar. Cerca de 1 M serve para "
       "quase todas as cenas pequenas; as grandes ou muito detalhadas pedem "
       "mais."),
    IT("Limite superiore al numero di gaussiane. Di più cattura più "
       "dettaglio, ma usa più VRAM e rende più lentamente. Circa 1 M va bene "
       "per quasi tutte le scene piccole; quelle grandi o molto dettagliate "
       "ne chiedono di più."),
    NL("Bovengrens voor het aantal gaussianen. Meer vangt meer detail, maar "
       "kost meer VRAM en rendert trager. Ongeveer 1 mln past bij de meeste "
       "kleine scènes; grote of zeer gedetailleerde hebben er meer nodig."),
    RU("Верхняя граница числа гауссиан. Больше — больше деталей, но и больше "
       "видеопамяти и медленнее отрисовка. Около 1 млн подходит большинству "
       "небольших сцен; крупным или очень детальным нужно больше."),
    TR("Gauss sayısının üst sınırı. Daha çoğu daha fazla ayrıntı yakalar ama "
       "daha çok VRAM kullanır ve daha yavaş işler. Küçük sahnelerin çoğuna "
       "~1 M yeter; büyük ya da çok ayrıntılı sahneler daha fazlasını "
       "isteyebilir."));

SS_MSG(opt_primitive,
    EN("Primitive"),     JA("プリミティブ"),   ZH_HANS("基元"),     ZH_HANT("基元"),
    KO("프리미티브"),     DE("Primitiv"),     FR("Primitive"),    ES("Primitiva"),
    PT("Primitiva"),     IT("Primitiva"),    NL("Primitief"),    RU("Примитив"),
    TR("İlkel"));

SS_MSG(opt_primitive_help,
    EN("Splat primitive. 3dgs: standard 3D Gaussian splatting, the most "
       "compatible with existing viewers. mip: anti-aliased Mip-Splatting, "
       "reduces shimmering when zooming out. 3dgut: Unscented-Transform "
       "projection, exact for distorted (fisheye/equirectangular) cameras."),
    JA("スプラットのプリミティブです。3dgs は標準の 3D ガウススプラッティング"
       "で、既存のビューアとの互換性がいちばん高い選択です。mip はアンチ"
       "エイリアスされた Mip-Splatting で、引いたときのちらつきを抑えます。"
       "3dgut は無香料変換による投影で、歪んだカメラ（魚眼・正距円筒）でも"
       "正確です。"),
    ZH_HANS("泼溅基元。3dgs：标准三维高斯泼溅，与现有查看器的兼容性最好。"
            "mip：抗锯齿的 Mip-Splatting，拉远时闪烁更少。3dgut：无迹变换投影，"
            "对畸变相机（鱼眼／等距柱状）是精确的。"),
    ZH_HANT("潑濺基元。3dgs：標準三維高斯潑濺，與現有檢視器的相容性最好。"
            "mip：抗鋸齒的 Mip-Splatting，拉遠時閃爍更少。3dgut：無跡變換投影，"
            "對變形相機（魚眼／等距柱狀）是精確的。"),
    KO("스플랫 프리미티브입니다. 3dgs: 표준 3D 가우시안 스플래팅으로, 기존 "
       "뷰어와의 호환성이 가장 좋습니다. mip: 앤티에일리어싱된 Mip-Splatting"
       "으로 축소할 때 깜빡임이 줄어듭니다. 3dgut: 무향 변환 투영으로, 왜곡된 "
       "카메라(어안·정거원통)에서도 정확합니다."),
    DE("Das Splat-Primitiv. 3dgs: klassisches 3D-Gaussian-Splatting, am "
       "besten mit vorhandenen Betrachtern verträglich. mip: "
       "kantengeglättetes Mip-Splatting, flimmert beim Herauszoomen weniger. "
       "3dgut: Projektion per Unscented Transform, exakt für verzeichnete "
       "Kameras (Fischauge / equirektangular)."),
    FR("La primitive de splat. 3dgs : Gaussian splatting 3D classique, le "
       "plus compatible avec les visionneuses existantes. mip : Mip-Splatting "
       "anticrénelé, scintille moins en dézoomant. 3dgut : projection par "
       "transformée non parfumée, exacte pour les caméras distordues "
       "(fisheye / équirectangulaire)."),
    ES("La primitiva de splat. 3dgs: Gaussian splatting 3D estándar, el más "
       "compatible con los visores existentes. mip: Mip-Splatting con "
       "antialiasing, parpadea menos al alejar. 3dgut: proyección por "
       "transformada no aromática, exacta para cámaras distorsionadas (ojo de "
       "pez / equirectangular)."),
    PT("A primitiva de splat. 3dgs: Gaussian splatting 3D padrão, o mais "
       "compatível com os visualizadores existentes. mip: Mip-Splatting com "
       "antisserrilhamento, cintila menos ao afastar. 3dgut: projeção por "
       "transformada unscented, exata para câmeras distorcidas (olho de peixe "
       "/ equirretangular)."),
    IT("La primitiva di splat. 3dgs: Gaussian splatting 3D classico, il più "
       "compatibile con i visualizzatori esistenti. mip: Mip-Splatting con "
       "antialiasing, sfarfalla meno allontanandosi. 3dgut: proiezione con "
       "trasformata unscented, esatta per fotocamere distorte (fisheye / "
       "equirettangolare)."),
    NL("De splat-primitief. 3dgs: standaard 3D Gaussian splatting, het best "
       "verenigbaar met bestaande viewers. mip: anti-aliased Mip-Splatting, "
       "flikkert minder bij uitzoomen. 3dgut: projectie via unscented "
       "transform, exact voor vervormde camera's (fisheye / equirectangulair)."),
    RU("Примитив сплата. 3dgs — обычный 3D Gaussian splatting, лучше всего "
       "совместимый с существующими просмотрщиками. mip — сглаженный "
       "Mip-Splatting, меньше мерцает при отдалении. 3dgut — проекция через "
       "сигма-точечное преобразование, точна для камер с искажениями (фишай, "
       "равнопромежуточная)."),
    TR("Splat ilkeli. 3dgs: standart 3B Gaussian splatting; var olan "
       "görüntüleyicilerle en uyumlu olanıdır. mip: kenar yumuşatmalı "
       "Mip-Splatting, uzaklaşırken daha az titrer. 3dgut: unscented dönüşüm "
       "izdüşümü, bozulmalı kameralar (balıkgözü / eşdikdörtgen) için tam "
       "doğrudur."));

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
    EN("Train at a fraction of the input resolution. Downscaling trains much "
       "faster and saves VRAM; use it for 4K+ footage or quick previews."),
    JA("入力解像度を落として学習します。縮小すると学習はずっと速くなり VRAM も"
       "節約できます。4K 以上の素材や、ざっと確認したいときに向きます。"),
    ZH_HANS("按输入分辨率的一部分来训练。缩小后训练快得多，也更省显存；"
            "适合 4K 以上素材或快速预览。"),
    ZH_HANT("按輸入解析度的一部分來訓練。縮小後訓練快得多，也更省顯示記憶體；"
            "適合 4K 以上素材或快速預覽。"),
    KO("입력 해상도의 일부만 써서 학습합니다. 축소하면 학습이 훨씬 빠르고 "
       "VRAM도 아낍니다. 4K 이상 소재나 빠른 미리보기에 알맞습니다."),
    DE("Mit einem Bruchteil der Eingangsauflösung trainieren. Herunterskalieren "
       "trainiert deutlich schneller und spart VRAM; sinnvoll für 4K-Material "
       "und schnelle Vorschauen."),
    FR("Entraîner à une fraction de la résolution d'entrée. Réduire accélère "
       "beaucoup l'entraînement et économise la VRAM ; utile pour des rushes "
       "en 4K et plus, ou pour un aperçu rapide."),
    ES("Entrenar a una fracción de la resolución de entrada. Reducir entrena "
       "mucho más rápido y ahorra VRAM; útil para material 4K o más, y para "
       "vistas previas rápidas."),
    PT("Treinar com uma fração da resolução de entrada. Reduzir treina muito "
       "mais rápido e economiza VRAM; útil para material 4K ou maior e para "
       "prévias rápidas."),
    IT("Addestrare a una frazione della risoluzione d'ingresso. Ridurre "
       "addestra molto più in fretta e risparmia VRAM; utile per materiale 4K "
       "o superiore e per anteprime rapide."),
    NL("Trainen op een fractie van de invoerresolutie. Verkleinen traint veel "
       "sneller en bespaart VRAM; handig bij 4K-materiaal of snelle "
       "voorbeelden."),
    RU("Обучать на доле исходного разрешения. Уменьшение сильно ускоряет "
       "обучение и экономит видеопамять; пригодится для материала 4K и выше "
       "или для быстрого просмотра."),
    TR("Girdi çözünürlüğünün bir kesrinde eğitin. Küçültmek eğitimi çok "
       "hızlandırır ve VRAM'den tasarruf ettirir; 4K ve üstü çekimler ya da "
       "hızlı önizleme için uygundur."));

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
    EN("What a mask means, where one is used. Ignore distractors: masked-out "
       "pixels are left out of the loss -- for people, cars, the "
       "photographer's shadow, or the area outside a fisheye circle. Cut out "
       "background: masked-out pixels are trained as empty, so the background "
       "is cut away and only the masked subject is reconstructed -- for "
       "object captures. Don't use masks: the mask folder is not read at "
       "all. Has no effect on a dataset without masks."),
    JA("マスクがある場合の意味です。「邪魔物を無視」ではマスクされた画素を"
       "損失から外します。通行人、車、撮影者の影、魚眼の円外といったものに"
       "向きます。「背景を切り抜く」ではマスクされた画素を空として学習するので、"
       "背景が取り除かれ、マスクされた被写体だけが再構成されます。物体の撮影"
       "向けです。「マスクを使わない」ではマスクを一切読み込みません。"
       "マスクのないデータセットでは効果はありません。"),
    ZH_HANS("有蒙版时蒙版的含义。“忽略干扰物”：被遮住的像素不计入损失——用于"
            "行人、汽车、摄影者的影子、鱼眼圆之外的区域。“裁掉背景”：被遮住的"
            "像素按空白训练，于是背景被裁掉，只重建被蒙版选中的主体——用于物体"
            "拍摄。“不使用蒙版”：完全不读取蒙版。数据集没有蒙版时不起作用。"),
    ZH_HANT("有遮罩時遮罩的含意。「忽略干擾物」：被遮住的像素不計入損失——用於"
            "行人、汽車、攝影者的影子、魚眼圓之外的區域。「裁掉背景」：被遮住的"
            "像素按空白訓練，於是背景被裁掉，只重建被遮罩選中的主體——用於物體"
            "拍攝。「不使用遮罩」：完全不讀取遮罩。資料集沒有遮罩時不起作用。"),
    KO("마스크가 있을 때 마스크의 의미입니다. 방해물 무시: 가려진 픽셀을 "
       "손실에서 뺍니다 — 지나가는 사람, 차, 촬영자의 그림자, 어안 원 바깥에 "
       "씁니다. 배경 잘라내기: 가려진 픽셀을 빈 곳으로 학습해 배경을 잘라내고 "
       "마스크된 피사체만 재구성합니다 — 물체 촬영용입니다. 마스크 사용 안 "
       "함: 마스크를 전혀 읽지 않습니다. 마스크가 없는 데이터셋에서는 아무 "
       "효과가 없습니다."),
    DE("Was eine Maske bedeutet, wo eine vorliegt. Störendes ignorieren: "
       "maskierte Pixel bleiben aus der Verlustfunktion heraus -- für "
       "Passanten, Autos, den eigenen Schatten oder den Bereich außerhalb des "
       "Fischaugenkreises. Hintergrund freistellen: maskierte Pixel werden als "
       "leer trainiert, der Hintergrund fällt weg und nur das maskierte Motiv "
       "wird rekonstruiert -- für Objektaufnahmen. Masken nicht verwenden: "
       "die Masken werden gar nicht erst gelesen. Ohne Masken im Datensatz "
       "ohne Wirkung."),
    FR("Ce que signifie un masque, là où il y en a un. Ignorer les gêneurs : "
       "les pixels masqués sont retirés de la fonction de coût -- pour les "
       "passants, les voitures, votre propre ombre ou la zone hors du cercle "
       "fisheye. Détourer l'arrière-plan : les pixels masqués sont entraînés "
       "comme vides, l'arrière-plan disparaît et seul le sujet masqué est "
       "reconstruit -- pour les prises d'objet. Ne pas utiliser de masques : "
       "les masques ne sont pas lus du tout. Sans effet sur un jeu de "
       "données sans masques."),
    ES("Qué significa una máscara, donde la hay. Ignorar los elementos "
       "molestos: los píxeles enmascarados quedan fuera de la función de "
       "pérdida -- para transeúntes, coches, la sombra del fotógrafo o la "
       "zona fuera del círculo de ojo de pez. Recortar el fondo: los píxeles "
       "enmascarados se entrenan como vacíos, el fondo se elimina y solo se "
       "reconstruye el sujeto enmascarado -- para capturas de objetos. No "
       "usar máscaras: las máscaras no se leen en absoluto. Sin efecto en un "
       "conjunto sin máscaras."),
    PT("O que uma máscara significa, onde houver uma. Ignorar o que atrapalha: "
       "os pixels mascarados ficam de fora da função de perda -- para pessoas "
       "passando, carros, a sombra do fotógrafo ou a área fora do círculo olho "
       "de peixe. Recortar o fundo: os pixels mascarados são treinados como "
       "vazios, o fundo é removido e só o sujeito mascarado é reconstruído -- "
       "para capturas de objetos. Não usar máscaras: as máscaras não são "
       "lidas de todo. Sem efeito num conjunto sem máscaras."),
    IT("Che cosa significa una maschera, dove ce n'è una. Ignorare i "
       "disturbi: i pixel mascherati restano fuori dalla funzione di perdita "
       "-- per passanti, automobili, l'ombra del fotografo o l'area fuori dal "
       "cerchio fisheye. Ritagliare lo sfondo: i pixel mascherati vengono "
       "addestrati come vuoti, lo sfondo sparisce e si ricostruisce solo il "
       "soggetto mascherato -- per le riprese di oggetti. Non usare le "
       "maschere: le maschere non vengono lette affatto. Senza effetto su un "
       "set di dati senza maschere."),
    NL("Wat een masker betekent, waar er een is. Storende dingen negeren: "
       "gemaskeerde pixels tellen niet mee in het verlies -- voor "
       "voorbijgangers, auto's, de schaduw van de fotograaf of het gebied "
       "buiten de fisheye-cirkel. Achtergrond uitsnijden: gemaskeerde pixels "
       "worden als leeg getraind, de achtergrond valt weg en alleen het "
       "gemaskeerde onderwerp wordt gereconstrueerd -- voor opnamen van een "
       "object. Maskers niet gebruiken: de maskers worden helemaal niet "
       "gelezen. Zonder maskers in de dataset zonder effect."),
    RU("Что означает маска там, где она есть. Игнорировать помехи: закрытые "
       "маской пиксели не входят в функцию потерь -- для прохожих, машин, "
       "тени фотографа или области вне круга «рыбьего глаза». Вырезать фон: "
       "закрытые маской пиксели обучаются как пустота, фон удаляется и "
       "восстанавливается только объект под маской -- для съёмки предметов. "
       "Не использовать маски: маски вообще не читаются. Без масок в наборе "
       "ни на что не влияет."),
    TR("Maske varsa maskenin ne anlama geldiği. Rahatsız edicileri yok say: "
       "maskelenen pikseller yitim işlevine girmez -- yoldan geçenler, "
       "arabalar, fotoğrafçının gölgesi ya da balıkgözü dairesinin dışı için. "
       "Arka planı ayır: maskelenen pikseller boş olarak eğitilir, arka plan "
       "kalkar ve yalnızca maskelenen konu yeniden oluşturulur -- nesne "
       "çekimleri için. Maskeleri kullanma: maskeler hiç okunmaz. Maskesiz "
       "bir veri kümesinde etkisi yoktur."));

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
    EN("Spherical-harmonics degree for view-dependent color (reflections, "
       "highlights). 3 is standard; 0 gives flat colors and the smallest "
       "model; 4 may have limited compatibility with mainstream viewers."),
    JA("視点によって変わる色（反射やハイライト）を表す球面調和関数の次数です。"
       "3 が標準。0 なら色は平坦になり、モデルは最小になります。4 は一般的な"
       "ビューアで表示できないことがあります。"),
    ZH_HANS("表示视角相关颜色（反射、高光）的球谐次数。3 是标准值；0 得到平坦"
            "的颜色和最小的模型；4 在主流查看器中的兼容性可能有限。"),
    ZH_HANT("表示視角相關顏色（反射、高光）的球諧次數。3 是標準值；0 得到平坦"
            "的顏色和最小的模型；4 在主流檢視器中的相容性可能有限。"),
    KO("시점에 따라 달라지는 색(반사, 하이라이트)을 나타내는 구면 조화 함수의 "
       "차수입니다. 3이 표준이고, 0이면 색이 평평해지며 모델이 가장 작아집니다. "
       "4는 일반 뷰어와의 호환성이 떨어질 수 있습니다."),
    DE("Grad der Kugelflächenfunktionen für blickabhängige Farbe (Reflexe, "
       "Glanzlichter). 3 ist Standard; 0 ergibt flache Farben und das "
       "kleinste Modell; 4 wird von verbreiteten Betrachtern womöglich nicht "
       "unterstützt."),
    FR("Degré des harmoniques sphériques pour la couleur dépendante du point "
       "de vue (reflets, spéculaires). 3 est la valeur standard ; 0 donne des "
       "couleurs plates et le plus petit modèle ; 4 peut ne pas être pris en "
       "charge par les visionneuses courantes."),
    ES("Grado de los armónicos esféricos para el color dependiente de la "
       "vista (reflejos, brillos). 3 es lo estándar; 0 da colores planos y el "
       "modelo más pequeño; 4 puede tener compatibilidad limitada con los "
       "visores habituales."),
    PT("Grau dos harmônicos esféricos para a cor dependente da vista "
       "(reflexos, brilhos). 3 é o padrão; 0 dá cores chapadas e o menor "
       "modelo; 4 pode ter compatibilidade limitada com visualizadores "
       "comuns."),
    IT("Grado delle armoniche sferiche per il colore dipendente dal punto di "
       "vista (riflessi, luci speculari). 3 è lo standard; 0 dà colori piatti "
       "e il modello più piccolo; 4 può avere compatibilità limitata con i "
       "visualizzatori diffusi."),
    NL("Graad van de sferische harmonischen voor kijkrichtingafhankelijke "
       "kleur (reflecties, highlights). 3 is standaard; 0 geeft vlakke "
       "kleuren en het kleinste model; 4 werkt mogelijk niet in gangbare "
       "viewers."),
    RU("Порядок сферических гармоник для цвета, зависящего от направления "
       "взгляда (отражения, блики). 3 — стандарт; 0 даёт плоские цвета и "
       "самую компактную модель; 4 может не поддерживаться распространёнными "
       "просмотрщиками."),
    TR("Bakış açısına bağlı renk (yansımalar, parlamalar) için küresel "
       "harmonik derecesi. 3 standarttır; 0 düz renkler ve en küçük modeli "
       "verir; 4 yaygın görüntüleyicilerde çalışmayabilir."));

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
    EN("Use a bilateral grid to correct color variation across images. "
       "Suitable for changing environment lighting. Uncheck for faster and "
       "more memory efficient training."),
    JA("画像ごとの色のばらつきをバイラテラルグリッドで補正します。環境光が"
       "変わる撮影に向きます。外すと学習は速く、メモリ効率もよくなります。"),
    ZH_HANS("用双边网格校正各张图像之间的颜色差异。适合环境光会变化的拍摄。"
            "取消勾选可让训练更快、更省内存。"),
    ZH_HANT("用雙邊網格校正各張影像之間的顏色差異。適合環境光會變化的拍攝。"
            "取消勾選可讓訓練更快、更省記憶體。"),
    KO("양방향 그리드로 이미지 간 색 편차를 보정합니다. 주변광이 변하는 촬영에 "
       "알맞습니다. 체크를 해제하면 학습이 더 빠르고 메모리도 덜 씁니다."),
    DE("Farbschwankungen zwischen den Bildern mit einem Bilateral Grid "
       "ausgleichen. Passend bei wechselndem Umgebungslicht. Abgeschaltet "
       "trainiert es schneller und speichersparender."),
    FR("Corriger les écarts de couleur entre les images avec une grille "
       "bilatérale. Adapté à un éclairage ambiant qui change. Décoché, "
       "l'entraînement est plus rapide et consomme moins de mémoire."),
    ES("Corregir la variación de color entre imágenes con una rejilla "
       "bilateral. Adecuado cuando cambia la luz ambiente. Sin marcar, el "
       "entrenamiento es más rápido y usa menos memoria."),
    PT("Corrigir a variação de cor entre as imagens com uma grade bilateral. "
       "Adequado quando a luz do ambiente muda. Desmarcado, o treinamento "
       "fica mais rápido e usa menos memória."),
    IT("Correggere la variazione di colore tra le immagini con una griglia "
       "bilaterale. Adatto quando la luce ambientale cambia. Deselezionato, "
       "l'addestramento è più rapido e usa meno memoria."),
    NL("Kleurverschillen tussen de beelden corrigeren met een bilateraal "
       "raster. Geschikt bij wisselend omgevingslicht. Uitgevinkt traint "
       "sneller en zuiniger met geheugen."),
    RU("Выравнивать различия цвета между снимками билатеральной сеткой. "
       "Подходит, когда меняется освещение. Без флажка обучение быстрее и "
       "экономнее по памяти."),
    TR("Görüntüler arasındaki renk farkını çift yönlü ızgarayla düzeltir. "
       "Ortam ışığının değiştiği çekimler için uygundur. İşareti kaldırmak "
       "eğitimi hızlandırır ve belleği daha az kullanır."));

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
    EN("Use PPISP to correct color variation across images. Suitable for "
       "camera vignetting and exposure/white-balance changes. Uncheck for "
       "faster training."),
    JA("画像ごとの色のばらつきを PPISP で補正します。レンズの周辺減光や、"
       "露出・ホワイトバランスの変化に向きます。外すと学習が速くなります。"),
    ZH_HANS("用 PPISP 校正各张图像之间的颜色差异。适合镜头暗角以及曝光／白平衡"
            "的变化。取消勾选可让训练更快。"),
    ZH_HANT("用 PPISP 校正各張影像之間的顏色差異。適合鏡頭暗角以及曝光／白平衡"
            "的變化。取消勾選可讓訓練更快。"),
    KO("PPISP로 이미지 간 색 편차를 보정합니다. 렌즈 비네팅과 노출·화이트밸런스 "
       "변화에 알맞습니다. 체크를 해제하면 학습이 더 빨라집니다."),
    DE("Farbschwankungen zwischen den Bildern mit PPISP ausgleichen. Passend "
       "bei Vignettierung und wechselnder Belichtung oder Weißabgleich. "
       "Abgeschaltet trainiert es schneller."),
    FR("Corriger les écarts de couleur entre les images avec PPISP. Adapté au "
       "vignetage et aux changements d'exposition ou de balance des blancs. "
       "Décoché, l'entraînement est plus rapide."),
    ES("Corregir la variación de color entre imágenes con PPISP. Adecuado "
       "para el viñeteo y los cambios de exposición o balance de blancos. Sin "
       "marcar, el entrenamiento es más rápido."),
    PT("Corrigir a variação de cor entre as imagens com PPISP. Adequado para "
       "vinhetagem e mudanças de exposição ou balanço de branco. Desmarcado, "
       "o treinamento fica mais rápido."),
    IT("Correggere la variazione di colore tra le immagini con PPISP. Adatto "
       "alla vignettatura e ai cambi di esposizione o bilanciamento del "
       "bianco. Deselezionato, l'addestramento è più rapido."),
    NL("Kleurverschillen tussen de beelden corrigeren met PPISP. Geschikt bij "
       "vignettering en wisselende belichting of witbalans. Uitgevinkt traint "
       "sneller."),
    RU("Выравнивать различия цвета между снимками с помощью PPISP. Подходит "
       "при виньетировании и изменениях экспозиции или баланса белого. Без "
       "флажка обучение быстрее."),
    TR("Görüntüler arasındaki renk farkını PPISP ile düzeltir. Vinyetleme ve "
       "pozlama / beyaz dengesi değişimleri için uygundur. İşareti kaldırmak "
       "eğitimi hızlandırır."));

SS_MSG(opt_distraction_warn,
    EN("Only enable distractor robustness for captures where people, vehicles "
       "or anything else moves between the photos and are not masked. On a "
       "clean capture it costs detail and gains nothing."),
    JA("「写り込みへの強さ」は、写真の間で人や車などが動き、それがマスクで"
       "除かれていない撮影のときだけ有効にしてください。きれいな撮影では、"
       "細部が失われるだけで効果はありません。"),
    ZH_HANS("只有当照片之间有行人、车辆等在移动，而且没有被蒙版遮住时，才开启"
            "“干扰物鲁棒性”。拍摄本身干净时，它只会损失细节而没有好处。"),
    ZH_HANT("只有當相片之間有行人、車輛等在移動，而且沒有被遮罩擋住時，才開啟"
            "「干擾物穩健度」。拍攝本身乾淨時，它只會損失細節而沒有好處。"),
    KO("사진 사이에 사람이나 차량 등이 움직이고 그것이 마스크로 가려져 있지 "
       "않은 촬영에서만 방해물 견고성을 켜세요. 깨끗한 촬영에서는 디테일만 "
       "잃고 얻는 것이 없습니다."),
    DE("\"Robustheit gegen Störobjekte\" nur für Aufnahmen einschalten, in "
       "denen sich Personen, Fahrzeuge oder anderes zwischen den Fotos "
       "bewegen und nicht maskiert sind. Bei einer sauberen Aufnahme kostet "
       "es Details und bringt nichts."),
    FR("N'activez « Robustesse aux intrus » que pour les prises de vue où des "
       "passants, des véhicules ou autre chose bougent d'une photo à l'autre "
       "sans être masqués. Sur une prise de vue propre, cela coûte du détail "
       "sans rien apporter."),
    ES("Activa «Robustez ante elementos molestos» solo en capturas donde "
       "personas, coches u otra cosa se mueven entre las fotos y no están "
       "enmascarados. En una captura limpia cuesta detalle y no aporta nada."),
    PT("Ative «Robustez a elementos indesejados» apenas em capturas em que "
       "pessoas, veículos ou outra coisa se movem entre as fotos e não estão "
       "mascarados. Em uma captura limpa, custa detalhe e não traz nada."),
    IT("Attiva «Robustezza agli elementi di disturbo» solo per riprese in cui "
       "persone, veicoli o altro si muovono da una foto all'altra e non sono "
       "mascherati. Su una ripresa pulita costa dettaglio e non porta nulla."),
    NL("Zet \"Bestandheid tegen stoorelementen\" alleen aan bij opnamen waarin "
       "mensen, voertuigen of iets anders tussen de foto's beweegt en niet "
       "gemaskeerd is. Bij een schone opname kost het detail en levert het "
       "niets op."),
    RU("Включайте «Устойчивость к помехам» только для съёмок, где между "
       "снимками движутся люди, машины или что-то ещё и они не скрыты маской. "
       "На чистой съёмке это лишь снижает детализацию и ничего не даёт."),
    TR("\"İstenmeyen nesnelere dayanıklılık\" seçeneğini yalnızca fotoğraflar "
       "arasında insanların, araçların ya da başka bir şeyin hareket ettiği "
       "ve maskelenmediği çekimlerde açın. Temiz bir çekimde ayrıntı "
       "kaybettirir, karşılığında bir şey vermez."));

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
    EN("End the run without writing anything else to disk: what stays in the "
       "output folder is what the last automatic save left there. The result "
       "is still loaded for viewing."),
    JA("ディスクには何も書き足さずに実行を終了します。出力フォルダに残るのは"
       "直前の自動保存までです。結果は表示用に読み込んだままです。"),
    ZH_HANS("结束这次训练，不再往磁盘写任何东西：输出文件夹里留下的仍是上一次"
            "自动保存的内容。结果仍留在内存中以便查看。"),
    ZH_HANT("結束這次訓練，不再往磁碟寫任何東西：輸出資料夾裡留下的仍是上一次"
            "自動儲存的內容。結果仍留在記憶體中以便檢視。"),
    KO("디스크에 더 쓰지 않고 실행을 끝냅니다. 출력 폴더에 남는 것은 직전 자동 "
       "저장까지입니다. 결과는 볼 수 있도록 그대로 둡니다."),
    DE("Beendet den Lauf, ohne noch etwas auf die Festplatte zu schreiben: im "
       "Ausgabeordner bleibt stehen, was die letzte automatische Speicherung "
       "dort gelassen hat. Das Ergebnis bleibt zum Betrachten geladen."),
    FR("Termine l'exécution sans plus rien écrire sur le disque : ce qui reste "
       "dans le dossier de sortie est ce que la dernière sauvegarde "
       "automatique y a laissé. Le résultat reste chargé pour être consulté."),
    ES("Termina la ejecución sin escribir nada más en el disco: en la carpeta "
       "de salida queda lo que dejó el último guardado automático. El "
       "resultado sigue cargado para verlo."),
    PT("Termina a execução sem escrever mais nada no disco: na pasta de saída "
       "fica o que o último salvamento automático deixou. O resultado "
       "continua carregado para visualização."),
    IT("Termina l'esecuzione senza scrivere altro su disco: nella cartella di "
       "uscita resta quello che ha lasciato l'ultimo salvataggio automatico. "
       "Il risultato resta caricato per guardarlo."),
    NL("Beëindigt de run zonder nog iets naar schijf te schrijven: in de "
       "uitvoermap blijft staan wat de laatste automatische opslag daar "
       "achterliet. Het resultaat blijft geladen om te bekijken."),
    RU("Завершает прогон, ничего больше не записывая на диск: в папке вывода "
       "останется то, что оставило последнее автоматическое сохранение. "
       "Результат остаётся загруженным для просмотра."),
    TR("Diske başka bir şey yazmadan çalışmayı bitirir: çıktı klasöründe son "
       "otomatik kaydın bıraktığı şey kalır. Sonuç görüntülemek için yüklü "
       "kalır."));

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
    EN("Finish the current step, save a checkpoint, and keep the result "
       "loaded for viewing."),
    JA("いまのステップを終えてチェックポイントを保存し、結果は表示用に"
       "読み込んだままにします。"),
    ZH_HANS("完成当前这一步，保存一个检查点，并把结果留在内存中以便查看。"),
    ZH_HANT("完成目前這一步，儲存一個檢查點，並把結果留在記憶體中以便檢視。"),
    KO("현재 단계를 마치고 체크포인트를 저장한 뒤, 결과는 볼 수 있도록 그대로 "
       "둡니다."),
    DE("Den laufenden Schritt zu Ende bringen, einen Prüfpunkt speichern und "
       "das Ergebnis zum Betrachten geladen lassen."),
    FR("Terminer l'étape en cours, enregistrer un point de sauvegarde et "
       "garder le résultat chargé pour le consulter."),
    ES("Terminar el paso actual, guardar un punto de control y dejar el "
       "resultado cargado para verlo."),
    PT("Terminar o passo atual, salvar um ponto de verificação e deixar o "
       "resultado carregado para visualização."),
    IT("Terminare il passo in corso, salvare un punto di controllo e lasciare "
       "il risultato caricato per poterlo osservare."),
    NL("De huidige stap afmaken, een controlepunt opslaan en het resultaat "
       "geladen laten om te bekijken."),
    RU("Завершить текущий шаг, сохранить контрольную точку и оставить "
       "результат загруженным для просмотра."),
    TR("Şu anki adımı bitir, bir denetim noktası kaydet ve sonucu görmek için "
       "yüklü bırak."));

// ---- status strip ----

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
SS_MSG(status_metrics,
    EN("splats: {0}   ssim: {1}   loss: {2}"),
    JA("スプラット: {0}   ssim: {1}   損失: {2}"),
    ZH_HANS("泼溅数：{0}   ssim：{1}   损失：{2}"),
    ZH_HANT("潑濺數：{0}   ssim：{1}   損失：{2}"),
    KO("스플랫: {0}   ssim: {1}   손실: {2}"),
    DE("Splats: {0}   ssim: {1}   Verlust: {2}"),
    FR("splats : {0}   ssim : {1}   perte : {2}"),
    ES("splats: {0}   ssim: {1}   pérdida: {2}"),
    PT("splats: {0}   ssim: {1}   perda: {2}"),
    IT("splat: {0}   ssim: {1}   perdita: {2}"),
    NL("splats: {0}   ssim: {1}   verlies: {2}"),
    RU("сплатов: {0}   ssim: {1}   потери: {2}"),
    TR("splat: {0}   ssim: {1}   kayıp: {2}"));

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
    EN("Dataset file unreadable"),
    JA("データセットのファイルを読めません"),
    ZH_HANS("无法读取数据集文件"),
    ZH_HANT("無法讀取資料集檔案"),
    KO("데이터셋 파일을 읽을 수 없음"),
    DE("Datensatzdatei nicht lesbar"),
    FR("Fichier du jeu de données illisible"),
    ES("Archivo del conjunto de datos ilegible"),
    PT("Ficheiro do conjunto de dados ilegível"),
    IT("File del set di dati illeggibile"),
    NL("Datasetbestand onleesbaar"),
    RU("Файл набора данных не читается"),
    TR("Veri kümesi dosyası okunamıyor"));

SS_MSG(data_error_intro,
    EN("Training is paused here and nothing is lost. Put the file back, then "
       "retry."),
    JA("ここで学習は一時停止していて、失われたものはありません。ファイルを"
       "戻してから、やり直してください。"),
    ZH_HANS("训练已在此暂停，没有任何损失。把文件放回去，然后重试。"),
    ZH_HANT("訓練已在此暫停，沒有任何損失。把檔案放回去，然後重試。"),
    KO("여기서 학습이 멈춰 있고 잃은 것은 없습니다. 파일을 되돌려 놓고 다시 "
       "시도하세요."),
    DE("Das Training pausiert hier, nichts geht verloren. Legen Sie die Datei "
       "zurück und versuchen Sie es erneut."),
    FR("L'entraînement est en pause ici et rien n'est perdu. Remettez le "
       "fichier en place, puis réessayez."),
    ES("El entrenamiento está en pausa aquí y no se pierde nada. Vuelve a "
       "poner el archivo y reinténtalo."),
    PT("O treinamento está pausado aqui e nada se perde. Coloque o ficheiro de "
       "volta e tente novamente."),
    IT("L'addestramento è in pausa qui e non si perde nulla. Rimetti a posto "
       "il file, poi riprova."),
    NL("De training staat hier stil en er gaat niets verloren. Zet het bestand "
       "terug en probeer het opnieuw."),
    RU("Обучение приостановлено здесь, ничего не потеряно. Верните файл на "
       "место и повторите."),
    TR("Eğitim burada duraklatıldı ve hiçbir şey kaybolmadı. Dosyayı geri "
       "koyup yeniden deneyin."));

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

SS_MSG(viewport_grid,
    EN("grid"),          JA("グリッド"),      ZH_HANS("网格"),     ZH_HANT("格線"),
    KO("격자"),           DE("Raster"),       FR("grille"),       ES("rejilla"),
    PT("grade"),         IT("griglia"),      NL("raster"),       RU("сетка"),
    TR("ızgara"));

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
    EN("Render resolution relative to the viewport size. Lower is faster and "
       "steals less time from training. Auto drops to half while you move the "
       "camera and goes back to full once it settles."),
    JA("ビューポートの大きさに対する描画解像度です。下げるほど速くなり、"
       "学習から奪う時間も減ります。「自動」はカメラを動かしている間は半分に"
       "落とし、止まると元に戻します。"),
    ZH_HANS("相对于视口尺寸的渲染分辨率。调低更快，也少占用训练时间。"
            "“自动”会在你移动相机时降到一半，停下后恢复。"),
    ZH_HANT("相對於檢視區尺寸的算繪解析度。調低更快，也少佔用訓練時間。"
            "「自動」會在你移動相機時降到一半，停下後恢復。"),
    KO("뷰포트 크기에 대한 렌더 해상도입니다. 낮출수록 빠르고 학습 시간을 덜 "
       "가져갑니다. \"자동\"은 카메라를 움직이는 동안 절반으로 낮췄다가 멈추면 "
       "원래대로 돌아갑니다."),
    DE("Renderauflösung relativ zur Fenstergröße. Niedriger ist schneller und "
       "nimmt dem Training weniger Zeit weg. Auto halbiert sie, solange Sie "
       "die Kamera bewegen, und geht zurück auf voll, sobald sie steht."),
    FR("Résolution de rendu par rapport à la taille de la vue. Plus bas est "
       "plus rapide et vole moins de temps à l'entraînement. Auto descend à la "
       "moitié pendant que vous déplacez la caméra et remonte quand elle "
       "s'arrête."),
    ES("Resolución de render respecto al tamaño de la vista. Más baja es más "
       "rápida y quita menos tiempo al entrenamiento. Auto baja a la mitad "
       "mientras mueve la cámara y vuelve a plena cuando se detiene."),
    PT("Resolução de renderização em relação ao tamanho da vista. Mais baixa "
       "é mais rápida e rouba menos tempo do treinamento. Auto cai para "
       "metade enquanto você move a câmera e volta ao cheio quando ela para."),
    IT("Risoluzione di rendering rispetto alla dimensione della vista. Più "
       "bassa è più rapida e ruba meno tempo all'addestramento. Auto scende a "
       "metà mentre muove la fotocamera e torna piena quando si ferma."),
    NL("Renderresolutie ten opzichte van de venstergrootte. Lager is sneller "
       "en kost de training minder tijd. Auto halveert zolang u de camera "
       "beweegt en gaat terug naar vol zodra die stilstaat."),
    RU("Разрешение отрисовки относительно размера окна. Ниже — быстрее и "
       "меньше отнимает времени у обучения. «Авто» снижает его вдвое, пока вы "
       "двигаете камеру, и возвращает полное, когда она замирает."),
    TR("Görünüm boyutuna göre işleme çözünürlüğü. Düşük olan daha hızlıdır ve "
       "eğitimden daha az zaman çalar. \"Oto\", kamerayı hareket ettirdiğiniz "
       "sürece yarıya iner ve durunca tama döner."));

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
    EN("Navigation mode (identical to the web viewer):\n"
       "{0} / {1} -- LMB orbit, RMB/MMB/Shift pan, wheel zoom\n"
       "{2} / {3} -- LMB look, WASD/arrows move, E/Q up-down "
       "({3}: E/Q roll)\nGamepad: left stick move, right stick look, "
       "triggers up-down/roll."),
    JA("操作モードです（ウェブビューアと同じ）:\n"
       "{0} / {1} -- 左ドラッグで回転、右・中ドラッグや Shift で"
       "平行移動、ホイールでズーム\n"
       "{2} / {3} -- 左ドラッグで視線、WASD／矢印で移動、E/Q で"
       "上下（{3}では E/Q はロール）\n"
       "ゲームパッド: 左スティックで移動、右スティックで視線、トリガーで"
       "上下・ロール。"),
    ZH_HANS("导航模式（与网页查看器一致）：\n"
            "{0} / {1} —— 左键环绕，右键／中键／Shift 平移，滚轮缩放\n"
            "{2} / {3} —— 左键转视角，WASD／方向键移动，E/Q 升降"
            "（{3}下 E/Q 为滚转）\n"
            "手柄：左摇杆移动，右摇杆转视角，扳机升降／滚转。"),
    ZH_HANT("導覽模式（與網頁檢視器一致）：\n"
            "{0} / {1} —— 左鍵環繞，右鍵／中鍵／Shift 平移，滾輪縮放\n"
            "{2} / {3} —— 左鍵轉視角，WASD／方向鍵移動，E/Q 升降"
            "（{3}下 E/Q 為滾轉）\n"
            "手把：左搖桿移動，右搖桿轉視角，扳機升降／滾轉。"),
    KO("이동 방식입니다(웹 뷰어와 동일):\n"
       "{0} / {1} -- 왼쪽 드래그로 궤도 회전, 오른쪽·가운데·Shift로 "
       "이동, 휠로 확대·축소\n"
       "{2} / {3} -- 왼쪽 드래그로 시선, WASD·화살표로 이동, "
       "E/Q로 상하({3}에서는 E/Q가 롤)\n"
       "게임패드: 왼쪽 스틱 이동, 오른쪽 스틱 시선, 트리거 상하·롤."),
    DE("Navigationsmodus (wie im Web-Betrachter):\n"
       "{0} / {1} -- linke Maustaste umkreisen, rechte/mittlere "
       "Taste oder Umschalt schwenken, Rad zoomen\n"
       "{2} / {3} -- linke Maustaste umsehen, WASD/Pfeile "
       "bewegen, E/Q hoch-runter ({3}: E/Q rollen)\n"
       "Gamepad: linker Stick bewegen, rechter Stick umsehen, Trigger "
       "hoch-runter/rollen."),
    FR("Mode de navigation (identique à la visionneuse web) :\n"
       "{0} / {1} -- clic gauche pour orbiter, clic droit/milieu "
       "ou Maj pour translater, molette pour zoomer\n"
       "{2} / {3} -- clic gauche pour regarder, WASD/flèches "
       "pour se déplacer, E/Q pour monter-descendre ({3} : E/Q roulis)\n"
       "Manette : stick gauche déplacement, stick droit regard, gâchettes "
       "montée-descente/roulis."),
    ES("Modo de navegación (igual que en el visor web):\n"
       "{0} / {1}: botón izquierdo para orbitar, derecho/central "
       "o Mayús para desplazar, rueda para acercar\n"
       "{2} / {3}: botón izquierdo para mirar, WASD/flechas "
       "para moverse, E/Q para subir y bajar (en {3}, E/Q alabean)\n"
       "Mando: stick izquierdo mover, stick derecho mirar, gatillos "
       "subir-bajar/alabear."),
    PT("Modo de navegação (igual ao visualizador web):\n"
       "{0} / {1} -- botão esquerdo orbita, direito/meio ou Shift "
       "desloca, roda aproxima\n"
       "{2} / {3} -- botão esquerdo olha, WASD/setas movem, E/Q "
       "sobem e descem (em {3}, E/Q rolam)\n"
       "Controle: analógico esquerdo move, direito olha, gatilhos "
       "sobem-descem/rolam."),
    IT("Modalità di navigazione (uguale al visualizzatore web):\n"
       "{0} / {1} -- tasto sinistro per orbitare, destro/centrale "
       "o Maiusc per traslare, rotellina per lo zoom\n"
       "{2} / {3} -- tasto sinistro per guardare, WASD/frecce "
       "per muoversi, E/Q su-giù (in {3} E/Q rollano)\n"
       "Gamepad: levetta sinistra movimento, destra sguardo, grilletti "
       "su-giù/rollio."),
    NL("Navigatiemodus (gelijk aan de webviewer):\n"
       "{0} / {1} -- linkermuisknop draaien, rechter/midden of "
       "Shift verschuiven, wiel zoomen\n"
       "{2} / {3} -- linkermuisknop kijken, WASD/pijlen "
       "bewegen, E/Q omhoog-omlaag ({3}: E/Q rollen)\n"
       "Gamepad: linkerstick bewegen, rechterstick kijken, triggers "
       "omhoog-omlaag/rollen."),
    RU("Режим навигации (как в веб-просмотрщике):\n"
       "{0} / {1} — левая кнопка вращает, правая/средняя или Shift "
       "сдвигают, колесо приближает\n"
       "{2} / {3} — левая кнопка поворачивает взгляд, WASD и "
       "стрелки перемещают, E/Q вверх-вниз (в режиме «{3}» E/Q — крен)\n"
       "Геймпад: левый стик — движение, правый — взгляд, триггеры — "
       "вверх-вниз и крен."),
    TR("Gezinme kipi (web görüntüleyicisiyle aynı):\n"
       "{0} / {1} -- sol tuş yörünge, sağ/orta tuş veya Shift "
       "kaydırma, tekerlek yakınlaştırma\n"
       "{2} / {3} -- sol tuş bakış, WASD/oklar hareket, E/Q "
       "yukarı-aşağı ({3} kipinde E/Q yalpalama)\n"
       "Oyun kolu: sol çubuk hareket, sağ çubuk bakış, tetikler "
       "yukarı-aşağı/yalpalama."));

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
    EN("View a Trained Model"),
    JA("学習済みモデルを見る"),
    ZH_HANS("查看已训练的模型"),
    ZH_HANT("檢視已訓練的模型"),
    KO("학습된 모델 보기"),
    DE("Trainiertes Modell ansehen"),
    FR("Voir un modèle entraîné"),
    ES("Ver un modelo entrenado"),
    PT("Ver um modelo treinado"),
    IT("Guarda un modello addestrato"),
    NL("Een getraind model bekijken"),
    RU("Посмотреть обученную модель"),
    TR("Eğitilmiş bir modeli görüntüle"));

SS_MSG(home_open_splat_help,
    EN("Open a .ply file, a checkpoint or a run folder and look around it. "
       "Models from other Gaussian splatting tools open too, and so does a "
       "plain point cloud."),
    JA("PLYファイル・チェックポイント・実行フォルダを開いて自由に見て回れます。"
       "他のガウススプラッティングツールのモデルや、ただの点群も開けます。"),
    ZH_HANS("打开 .ply 文件、检查点或运行文件夹，随意观察。也可以打开其他高斯泼溅"
            "工具的模型，以及普通点云。"),
    ZH_HANT("開啟 .ply 檔案、檢查點或執行資料夾，隨意觀察。也可以開啟其他高斯潑濺"
            "工具的模型，以及一般點雲。"),
    KO(".ply 파일, 체크포인트, 실행 폴더를 열어 자유롭게 둘러봅니다. 다른 가우시안 "
       "스플래팅 도구의 모델이나 단순한 점군도 열립니다."),
    DE("Eine .ply-Datei, einen Prüfpunkt oder einen Laufordner öffnen und sich "
       "darin umsehen. Modelle aus anderen Gaussian-Splatting-Werkzeugen lassen "
       "sich ebenso öffnen wie eine reine Punktwolke."),
    FR("Ouvrez un fichier .ply, un point de sauvegarde ou un dossier "
       "d'exécution et promenez-vous dedans. Les modèles d'autres outils de "
       "Gaussian splatting s'ouvrent aussi, tout comme un simple nuage de points."),
    ES("Abra un archivo .ply, un punto de control o una carpeta de ejecución y "
       "recórralo. También se abren modelos de otras herramientas de Gaussian "
       "splatting y una simple nube de puntos."),
    PT("Abra um arquivo .ply, um ponto de verificação ou uma pasta de execução "
       "e percorra-o. Modelos de outras ferramentas de Gaussian splatting "
       "também abrem, assim como uma simples nuvem de pontos."),
    IT("Apra un file .ply, un punto di controllo o una cartella di esecuzione e "
       "ci si muova dentro. Si aprono anche i modelli di altri strumenti di "
       "Gaussian splatting e una semplice nuvola di punti."),
    NL("Open een .ply-bestand, een checkpoint of een uitvoermap en kijk erin "
       "rond. Modellen uit andere Gaussian-splattingprogramma's openen ook, "
       "net als een gewone puntenwolk."),
    RU("Откройте файл .ply, контрольную точку или папку запуска и осмотритесь. "
       "Модели из других инструментов гауссова сплаттинга тоже открываются, как "
       "и обычное облако точек."),
    TR("Bir .ply dosyasını, bir kontrol noktasını veya bir çalışma klasörünü "
       "açıp içinde gezinin. Başka Gaussian splatting araçlarının modelleri de, "
       "sıradan bir nokta bulutu da açılır."));

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
    EN("Two reconstructions of one scene share coordinates, so this lines them "
       "up exactly. Turn it off for a model that has nothing to do with the "
       "first one, and it is framed on its own instead."),
    JA("同じシーンを再構成した2つのモデルは座標を共有するので、これでぴったり"
       "重なります。最初のモデルと無関係なモデルではオフにすると、それ自体に"
       "合わせて表示されます。"),
    ZH_HANS("同一场景的两次重建共用坐标，打开它就能精确对齐。若这个模型与第一个"
            "毫无关系，请关掉它，模型会按自身范围取景。"),
    ZH_HANT("同一場景的兩次重建共用座標，開啟它就能精確對齊。若這個模型與第一個"
            "毫無關係，請關掉它，模型會依自身範圍取景。"),
    KO("같은 장면을 두 번 복원한 모델은 좌표가 같으므로 이것으로 정확히 "
       "겹칩니다. 첫 모델과 무관한 모델이라면 꺼 두세요. 그러면 그 모델 자체에 "
       "맞춰 보여 줍니다."),
    DE("Zwei Rekonstruktionen einer Szene teilen sich die Koordinaten, das "
       "bringt sie genau zur Deckung. Für ein Modell, das mit dem ersten "
       "nichts zu tun hat, ausschalten -- dann wird es für sich gerahmt."),
    FR("Deux reconstructions d'une même scène partagent leurs coordonnées : "
       "cela les superpose exactement. Désactivez-le pour un modèle sans "
       "rapport avec le premier, il sera alors cadré pour lui-même."),
    ES("Dos reconstrucciones de una misma escena comparten coordenadas, así "
       "que esto las superpone con exactitud. Desactívelo para un modelo que "
       "no tenga que ver con el primero: se encuadrará por sí solo."),
    PT("Duas reconstruções de uma mesma cena partilham coordenadas, então isto "
       "as sobrepõe com exatidão. Desligue para um modelo que nada tenha a ver "
       "com o primeiro: ele será enquadrado por si só."),
    IT("Due ricostruzioni della stessa scena condividono le coordinate, quindi "
       "così si sovrappongono esattamente. Lo disattivi per un modello che con "
       "il primo non c'entra: verrà inquadrato per conto suo."),
    NL("Twee reconstructies van dezelfde scène delen coördinaten, dus hiermee "
       "vallen ze precies samen. Zet het uit voor een model dat niets met het "
       "eerste te maken heeft; dat wordt dan op zichzelf ingekaderd."),
    RU("Две реконструкции одной сцены имеют общие координаты, поэтому так они "
       "совпадут точно. Для модели, не связанной с первой, выключите -- она "
       "будет вписана сама по себе."),
    TR("Aynı sahnenin iki yeniden oluşturması koordinatları paylaşır, bu da "
       "onları tam olarak üst üste getirir. İlkiyle ilgisi olmayan bir model "
       "için kapatın; o zaman kendi başına çerçevelenir."));

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
    EN("Write every option on this screen to a preset file, so the same settings "
       "can be loaded onto another dataset or pointed at from a batch run. The "
       "dataset and the output folder are not part of a preset."),
    JA("この画面の設定をすべてプリセットファイルに書き出します。同じ設定を別の"
       "データセットに読み込んだり、バッチ実行から指定したりできます。"
       "データセットと出力先フォルダーはプリセットに含まれません。"),
    ZH_HANS("把这个页面上的所有选项写入一个预设文件，这样同一套设置就能加载到"
            "别的数据集上，或者在批量训练里直接指向它。数据集和输出文件夹不属于"
            "预设的一部分。"),
    ZH_HANT("把這個頁面上的所有選項寫入一個預設檔，這樣同一套設定就能載入到"
            "別的資料集上，或者在批次訓練裡直接指向它。資料集和輸出資料夾不屬於"
            "預設的一部分。"),
    KO("이 화면의 모든 옵션을 프리셋 파일로 씁니다. 같은 설정을 다른 데이터셋에 "
       "불러오거나 일괄 실행에서 가리킬 수 있습니다. 데이터셋과 출력 폴더는 "
       "프리셋에 들어가지 않습니다."),
    DE("Alle Einstellungen dieses Bildschirms in eine Voreinstellungsdatei "
       "schreiben, damit dieselben Werte auf einen anderen Datensatz geladen "
       "oder aus einem Stapellauf angesteuert werden können. Datensatz und "
       "Ausgabeordner gehören nicht dazu."),
    FR("Écrire toutes les options de cet écran dans un fichier de préréglage, "
       "pour charger les mêmes valeurs sur un autre jeu de données ou les viser "
       "depuis un traitement par lots. Le jeu de données et le dossier de "
       "sortie n'en font pas partie."),
    ES("Escribir todas las opciones de esta pantalla en un archivo de ajuste, "
       "para cargar los mismos valores en otro conjunto de datos o apuntar a "
       "ellos desde una ejecución por lotes. El conjunto de datos y la carpeta "
       "de salida no forman parte del ajuste."),
    PT("Escrever todas as opções desta tela em um arquivo de predefinição, para "
       "carregar os mesmos valores em outro conjunto de dados ou apontar para "
       "eles a partir de uma execução em lote. O conjunto de dados e a pasta de "
       "saída não fazem parte da predefinição."),
    IT("Scrivere tutte le opzioni di questa schermata in un file di "
       "preimpostazione, così gli stessi valori si possono caricare su un altro "
       "set di dati o richiamare da un'esecuzione in batch. Il set di dati e la "
       "cartella di uscita non ne fanno parte."),
    NL("Alle opties op dit scherm naar een voorinstellingsbestand schrijven, "
       "zodat dezelfde waarden op een andere dataset geladen of vanuit een "
       "batchrun aangewezen kunnen worden. De dataset en de uitvoermap horen er "
       "niet bij."),
    RU("Записать все параметры этого экрана в файл пресета, чтобы те же "
       "значения можно было загрузить на другой набор данных или указать из "
       "пакетного запуска. Набор данных и папка вывода в пресет не входят."),
    TR("Bu ekrandaki bütün seçenekleri bir hazır ayar dosyasına yazar; böylece "
       "aynı değerler başka bir veri kümesine yüklenebilir ya da toplu "
       "çalıştırmadan gösterilebilir. Veri kümesi ve çıktı klasörü hazır ayara "
       "dahil değildir."));
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
    EN("Read a preset file, or the config.json of a run that came out well. "
       "Options the file does not mention come up at their defaults, and "
       "options it names that this build does not have are ignored."),
    JA("プリセットファイル、またはうまくいった実行の config.json を読み込みます。"
       "ファイルに書かれていない設定は既定値になり、このビルドに存在しない設定名は"
       "無視されます。"),
    ZH_HANS("读取一个预设文件，或者某次效果不错的运行留下的 config.json。文件里"
            "没提到的选项按默认值来，文件里提到但本版本没有的选项会被忽略。"),
    ZH_HANT("讀取一個預設檔，或者某次效果不錯的執行留下的 config.json。檔案裡"
            "沒提到的選項按預設值來，檔案裡提到但本版本沒有的選項會被忽略。"),
    KO("프리셋 파일이나 결과가 좋았던 실행의 config.json을 읽습니다. 파일에 없는 "
       "옵션은 기본값으로 뜨고, 파일에 있지만 이 빌드에 없는 옵션은 무시합니다."),
    DE("Eine Voreinstellungsdatei lesen -- oder die config.json eines Laufs, "
       "der gut geworden ist. Optionen, die die Datei nicht nennt, stehen auf "
       "ihrem Standard; Optionen, die sie nennt und die es in diesem Build "
       "nicht gibt, werden übergangen."),
    FR("Lire un fichier de préréglage, ou le config.json d'une exécution "
       "réussie. Les options absentes du fichier reprennent leur valeur par "
       "défaut, et celles qu'il nomme mais qui n'existent pas dans cette "
       "version sont ignorées."),
    ES("Leer un archivo de ajuste, o el config.json de una ejecución que salió "
       "bien. Las opciones que el archivo no menciona quedan en su valor por "
       "defecto, y las que nombra pero no existen en esta versión se ignoran."),
    PT("Ler um arquivo de predefinição, ou o config.json de uma execução que "
       "saiu bem. As opções que o arquivo não menciona ficam no valor padrão, e "
       "as que ele nomeia mas não existem nesta versão são ignoradas."),
    IT("Legge un file di preimpostazione, o il config.json di un'esecuzione "
       "riuscita. Le opzioni che il file non nomina restano al valore "
       "predefinito, e quelle che nomina ma che questa build non ha vengono "
       "ignorate."),
    NL("Een voorinstellingsbestand lezen, of de config.json van een run die "
       "goed uitpakte. Opties die het bestand niet noemt, komen op hun "
       "standaardwaarde; opties die het wel noemt maar die deze build niet "
       "heeft, worden genegeerd."),
    RU("Прочитать файл пресета или config.json удачного запуска. Параметры, "
       "которых в файле нет, останутся со значением по умолчанию, а названные в "
       "нём параметры, которых нет в этой сборке, будут пропущены."),
    TR("Bir hazır ayar dosyasını ya da iyi sonuç veren bir çalıştırmanın "
       "config.json dosyasını okur. Dosyada geçmeyen seçenekler varsayılan "
       "değerleriyle gelir; dosyanın adını verdiği ama bu sürümde bulunmayan "
       "seçenekler yok sayılır."));
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
    EN("Save Training Preset"),
    JA("学習プリセットを保存"),
    ZH_HANS("保存训练预设"),
    ZH_HANT("儲存訓練預設"),
    KO("학습 프리셋 저장"),
    DE("Trainingsvoreinstellung speichern"),
    FR("Enregistrer le préréglage d'entraînement"),
    ES("Guardar el ajuste de entrenamiento"),
    PT("Salvar a predefinição de treinamento"),
    IT("Salva la preimpostazione di addestramento"),
    NL("Trainingsvoorinstelling opslaan"),
    RU("Сохранение пресета обучения"),
    TR("Eğitim hazır ayarını kaydet"));
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
    EN("Remove the selected preset's file. The options on screen are left as "
       "they are -- this deletes the saved copy, not what you are working on."),
    JA("選んでいるプリセットのファイルを消します。画面上の設定はそのままです。"
       "消えるのは保存された控えであって、いま編集中の内容ではありません。"),
    ZH_HANS("删除所选预设对应的文件。屏幕上的选项保持不变——删掉的是保存下来的"
            "那份副本，不是你正在改的内容。"),
    ZH_HANT("刪除所選預設對應的檔案。畫面上的選項保持不變——刪掉的是儲存下來的"
            "那份副本，不是你正在改的內容。"),
    KO("선택한 프리셋의 파일을 지웁니다. 화면의 옵션은 그대로 둡니다. 지워지는 "
       "것은 저장해 둔 사본이지, 지금 작업 중인 내용이 아닙니다."),
    DE("Die Datei der gewählten Voreinstellung entfernen. Die Einstellungen auf "
       "dem Bildschirm bleiben, wie sie sind -- gelöscht wird die gespeicherte "
       "Kopie, nicht das, woran Sie gerade arbeiten."),
    FR("Supprimer le fichier du préréglage sélectionné. Les options à l'écran "
       "ne bougent pas : c'est la copie enregistrée qui disparaît, pas ce sur "
       "quoi vous travaillez."),
    ES("Eliminar el archivo del ajuste seleccionado. Las opciones en pantalla "
       "no cambian: se borra la copia guardada, no aquello en lo que estás "
       "trabajando."),
    PT("Remover o arquivo da predefinição selecionada. As opções na tela ficam "
       "como estão: some a cópia salva, não aquilo em que você está "
       "trabalhando."),
    IT("Elimina il file della preimpostazione selezionata. Le opzioni a schermo "
       "restano come sono: sparisce la copia salvata, non ciò su cui stai "
       "lavorando."),
    NL("Het bestand van de gekozen voorinstelling verwijderen. De opties op het "
       "scherm blijven staan -- weg gaat de opgeslagen kopie, niet waar je mee "
       "bezig bent."),
    RU("Удалить файл выбранного пресета. Параметры на экране останутся как "
       "есть: пропадает сохранённая копия, а не то, над чем вы работаете."),
    TR("Seçili hazır ayarın dosyasını siler. Ekrandaki seçenekler olduğu gibi "
       "kalır -- silinen, kaydedilmiş kopyadır; üzerinde çalıştığınız şey "
       "değil."));
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
// Batch training
// ===========================================================================

SS_MSG(home_batch,
    EN("Batch Training"),
    JA("バッチ学習"),
    ZH_HANS("批量训练"),
    ZH_HANT("批次訓練"),
    KO("일괄 학습"),
    DE("Stapeltraining"),
    FR("Entraînement par lots"),
    ES("Entrenamiento por lotes"),
    PT("Treinamento em lote"),
    IT("Addestramento in batch"),
    NL("Batchtraining"),
    RU("Пакетное обучение"),
    TR("Toplu eğitim"));
SS_MSG(home_batch_help,
    EN("Queue several datasets, each with its own preset, and train them one "
       "after another without supervision."),
    JA("複数のデータセットをそれぞれのプリセットとともに並べて、順番に学習させ"
       "ます。付きっきりでいる必要はありません。"),
    ZH_HANS("把多个数据集排成队列，各自配一个预设，然后一个接一个训练下去，中途"
            "不用盯着。"),
    ZH_HANT("把多個資料集排成佇列，各自配一個預設，然後一個接一個訓練下去，中途"
            "不用盯著。"),
    KO("여러 데이터셋을 각자의 프리셋과 함께 줄 세워 두면, 지켜보지 않아도 차례로 "
       "학습합니다."),
    DE("Mehrere Datensätze mit je eigener Voreinstellung in eine Warteschlange "
       "stellen und unbeaufsichtigt nacheinander trainieren."),
    FR("Mettre en file plusieurs jeux de données, chacun avec son préréglage, "
       "et les entraîner l'un après l'autre sans surveillance."),
    ES("Poner en cola varios conjuntos de datos, cada uno con su ajuste, y "
       "entrenarlos uno tras otro sin vigilarlos."),
    PT("Enfileirar vários conjuntos de dados, cada um com sua predefinição, e "
       "treiná-los um após o outro sem supervisão."),
    IT("Mettere in coda più set di dati, ciascuno con la sua preimpostazione, e "
       "addestrarli uno dopo l'altro senza sorvegliarli."),
    NL("Meerdere datasets in de wachtrij zetten, elk met een eigen "
       "voorinstelling, en ze zonder toezicht na elkaar trainen."),
    RU("Поставить в очередь несколько наборов данных, каждый со своим пресетом, "
       "и обучать их один за другим без присмотра."),
    TR("Her biri kendi hazır ayarıyla birkaç veri kümesini sıraya koyup, başında "
       "durmadan arka arkaya eğitir."));
SS_MSG(menu_batch,
    EN("Batch Training..."),
    JA("バッチ学習…"),
    ZH_HANS("批量训练…"),
    ZH_HANT("批次訓練…"),
    KO("일괄 학습…"),
    DE("Stapeltraining …"),
    FR("Entraînement par lots…"),
    ES("Entrenamiento por lotes…"),
    PT("Treinamento em lote…"),
    IT("Addestramento in batch…"),
    NL("Batchtraining…"),
    RU("Пакетное обучение…"),
    TR("Toplu eğitim…"));
SS_MSG(batch_title,
    EN("Batch Training"),
    JA("バッチ学習"),
    ZH_HANS("批量训练"),
    ZH_HANT("批次訓練"),
    KO("일괄 학습"),
    DE("Stapeltraining"),
    FR("Entraînement par lots"),
    ES("Entrenamiento por lotes"),
    PT("Treinamento em lote"),
    IT("Addestramento in batch"),
    NL("Batchtraining"),
    RU("Пакетное обучение"),
    TR("Toplu eğitim"));
SS_MSG(batch_intro,
    EN("Each row trains one dataset with one preset, top to bottom. A row that "
       "fails is recorded and the next one starts anyway."),
    JA("1 行につきデータセット 1 つをプリセット 1 つで、上から順に学習します。"
       "失敗した行は記録され、次の行はそのまま始まります。"),
    ZH_HANS("每一行用一个预设训练一个数据集，从上往下依次进行。失败的行会记录"
            "下来，下一行照常开始。"),
    ZH_HANT("每一行用一個預設訓練一個資料集，從上往下依序進行。失敗的行會記錄"
            "下來，下一行照常開始。"),
    KO("한 행이 데이터셋 하나를 프리셋 하나로 학습하며, 위에서 아래로 진행합니다. "
       "실패한 행은 기록해 두고 다음 행을 그대로 시작합니다."),
    DE("Jede Zeile trainiert einen Datensatz mit einer Voreinstellung, von oben "
       "nach unten. Eine gescheiterte Zeile wird vermerkt, die nächste startet "
       "trotzdem."),
    FR("Chaque ligne entraîne un jeu de données avec un préréglage, de haut en "
       "bas. Une ligne en échec est notée et la suivante démarre quand même."),
    ES("Cada fila entrena un conjunto de datos con un ajuste, de arriba abajo. "
       "Una fila que falla queda anotada y la siguiente arranca igualmente."),
    PT("Cada linha treina um conjunto de dados com uma predefinição, de cima "
       "para baixo. Uma linha que falha fica registrada e a seguinte começa "
       "assim mesmo."),
    IT("Ogni riga addestra un set di dati con una preimpostazione, dall'alto in "
       "basso. Una riga fallita viene annotata e la successiva parte comunque."),
    NL("Elke rij traint één dataset met één voorinstelling, van boven naar "
       "beneden. Een rij die mislukt wordt genoteerd en de volgende start toch."),
    RU("Каждая строка обучает один набор данных с одним пресетом, сверху вниз. "
       "Сорвавшаяся строка записывается, а следующая всё равно запускается."),
    TR("Her satır bir veri kümesini bir hazır ayarla, yukarıdan aşağıya eğitir. "
       "Başarısız olan satır kaydedilir ve sıradaki yine de başlar."));
SS_MSG(batch_drop_hint,
    EN("Drop dataset folders here to add them."),
    JA("データセットのフォルダーをここにドロップすると追加されます。"),
    ZH_HANS("把数据集文件夹拖到这里即可添加。"),
    ZH_HANT("把資料集資料夾拖到這裡即可新增。"),
    KO("데이터셋 폴더를 여기에 끌어다 놓으면 추가됩니다."),
    DE("Datensatzordner hierher ziehen, um sie hinzuzufügen."),
    FR("Déposez ici des dossiers de jeux de données pour les ajouter."),
    ES("Arrastra aquí carpetas de conjuntos de datos para añadirlas."),
    PT("Arraste pastas de conjuntos de dados até aqui para adicioná-las."),
    IT("Trascina qui le cartelle dei set di dati per aggiungerle."),
    NL("Sleep datasetmappen hierheen om ze toe te voegen."),
    RU("Перетащите сюда папки наборов данных, чтобы добавить их."),
    TR("Veri kümesi klasörlerini eklemek için buraya bırakın."));
SS_MSG(batch_empty,
    EN("The list is empty. Add a dataset to get started."),
    JA("一覧が空です。まずデータセットを追加してください。"),
    ZH_HANS("列表是空的。先添加一个数据集吧。"),
    ZH_HANT("清單是空的。先新增一個資料集吧。"),
    KO("목록이 비어 있습니다. 데이터셋을 하나 추가해 보세요."),
    DE("Die Liste ist leer. Fügen Sie zum Start einen Datensatz hinzu."),
    FR("La liste est vide. Ajoutez un jeu de données pour commencer."),
    ES("La lista está vacía. Añade un conjunto de datos para empezar."),
    PT("A lista está vazia. Adicione um conjunto de dados para começar."),
    IT("L'elenco è vuoto. Aggiungi un set di dati per iniziare."),
    NL("De lijst is leeg. Voeg een dataset toe om te beginnen."),
    RU("Список пуст. Добавьте набор данных, чтобы начать."),
    TR("Liste boş. Başlamak için bir veri kümesi ekleyin."));
SS_MSG(batch_add_row,
    EN("Add dataset..."),
    JA("データセットを追加…"),
    ZH_HANS("添加数据集…"),
    ZH_HANT("新增資料集…"),
    KO("데이터셋 추가…"),
    DE("Datensatz hinzufügen …"),
    FR("Ajouter un jeu de données…"),
    ES("Añadir un conjunto de datos…"),
    PT("Adicionar conjunto de dados…"),
    IT("Aggiungi set di dati…"),
    NL("Dataset toevoegen…"),
    RU("Добавить набор данных…"),
    TR("Veri kümesi ekle…"));
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
    EN("Look for the reasons a row is going to fail -- a folder with no "
       "reconstruction in it, a preset that has gone missing, an option the "
       "trainer does not implement -- without starting anything."),
    JA("何も動かさずに、行が失敗しそうな理由を探します。再構成が入っていない"
       "フォルダー、なくなったプリセット、学習側が実装していない設定などです。"),
    ZH_HANS("先不启动任何东西，只找出各行会失败的原因：文件夹里没有重建结果、"
            "预设文件不见了、用到了训练端没实现的选项等等。"),
    ZH_HANT("先不啟動任何東西，只找出各行會失敗的原因：資料夾裡沒有重建結果、"
            "預設檔不見了、用到了訓練端沒實作的選項等等。"),
    KO("아무것도 시작하지 않고, 행이 실패할 만한 이유를 찾습니다. 재구성이 없는 "
       "폴더, 사라진 프리셋, 학습기가 구현하지 않은 옵션 같은 것들입니다."),
    DE("Ohne etwas zu starten nach den Gründen suchen, aus denen eine Zeile "
       "scheitern wird: ein Ordner ohne Rekonstruktion, eine verschwundene "
       "Voreinstellung, eine Option, die das Training nicht umsetzt."),
    FR("Chercher, sans rien lancer, les raisons pour lesquelles une ligne va "
       "échouer : un dossier sans reconstruction, un préréglage disparu, une "
       "option que l'entraînement n'implémente pas."),
    ES("Buscar, sin arrancar nada, los motivos por los que una fila va a "
       "fallar: una carpeta sin reconstrucción, un ajuste que ha desaparecido, "
       "una opción que el entrenamiento no implementa."),
    PT("Procurar, sem iniciar nada, os motivos pelos quais uma linha vai "
       "falhar: uma pasta sem reconstrução, uma predefinição que sumiu, uma "
       "opção que o treinamento não implementa."),
    IT("Cercare, senza avviare nulla, i motivi per cui una riga fallirà: una "
       "cartella senza ricostruzione, una preimpostazione sparita, un'opzione "
       "che l'addestramento non implementa."),
    NL("Zonder iets te starten zoeken naar de redenen waarom een rij zal "
       "mislukken: een map zonder reconstructie, een verdwenen voorinstelling, "
       "een optie die de training niet kent."),
    RU("Не запуская ничего, найти причины, по которым строка сорвётся: папка без "
       "реконструкции, пропавший пресет, параметр, который обучение не "
       "поддерживает."),
    TR("Hiçbir şey başlatmadan, bir satırın neden başarısız olacağını arar: "
       "içinde yeniden oluşturma bulunmayan bir klasör, kaybolmuş bir hazır "
       "ayar, eğitimin gerçeklemediği bir seçenek."));
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
SS_MSG(batch_col_dataset,
    EN("Dataset"),       JA("データセット"),   ZH_HANS("数据集"),   ZH_HANT("資料集"),
    KO("데이터셋"),        DE("Datensatz"),    FR("Jeu de données"),
    ES("Conjunto de datos"), PT("Conjunto de dados"), IT("Set di dati"),
    NL("Dataset"),       RU("Набор данных"), TR("Veri kümesi"));
SS_MSG(batch_col_preset,
    EN("Preset"),        JA("プリセット"),     ZH_HANS("预设"),     ZH_HANT("預設"),
    KO("프리셋"),         DE("Voreinstellung"), FR("Préréglage"),  ES("Ajuste"),
    PT("Predefinição"),  IT("Preimpostazione"), NL("Voorinstelling"),
    RU("Пресет"),        TR("Hazır ayar"));
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
SS_MSG(batch_col_status,
    EN("Status"),        JA("状態"),          ZH_HANS("状态"),     ZH_HANT("狀態"),
    KO("상태"),           DE("Status"),       FR("État"),         ES("Estado"),
    PT("Estado"),        IT("Stato"),        NL("Status"),       RU("Состояние"),
    TR("Durum"));
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
// The three columns that save making a near-identical preset for every
// combination of the numbers people actually change.
SS_MSG(batch_override_hint,
    EN("preset"),        JA("プリセット"),     ZH_HANS("预设"),     ZH_HANT("預設"),
    KO("프리셋"),         DE("Voreinstellung"), FR("préréglage"),  ES("ajuste"),
    PT("predefinição"),  IT("preimpostazione"), NL("voorinstelling"),
    RU("пресет"),        TR("hazır ayar"));
SS_MSG(batch_override_help,
    EN("Overrides what the preset says, for this row alone. Leave it empty to "
       "train with the preset's own value."),
    JA("この行だけ、プリセットの値を上書きします。空にしておくとプリセットの値の"
       "まま学習します。"),
    ZH_HANS("只对这一行覆盖预设里的值。留空就按预设本身的值来训练。"),
    ZH_HANT("只對這一行覆寫預設裡的值。留空就按預設本身的值來訓練。"),
    KO("이 행에 한해 프리셋 값을 덮어씁니다. 비워 두면 프리셋의 값 그대로 "
       "학습합니다."),
    DE("Überschreibt nur für diese Zeile, was die Voreinstellung sagt. Leer "
       "lassen, um mit deren eigenem Wert zu trainieren."),
    FR("Remplace ce que dit le préréglage, pour cette ligne seulement. Laisser "
       "vide pour entraîner avec la valeur du préréglage."),
    ES("Sustituye lo que dice el ajuste, solo en esta fila. Déjalo vacío para "
       "entrenar con el valor del propio ajuste."),
    PT("Substitui o que a predefinição diz, só nesta linha. Deixe vazio para "
       "treinar com o valor da própria predefinição."),
    IT("Sovrascrive quanto dice la preimpostazione, solo per questa riga. "
       "Lascialo vuoto per addestrare con il valore della preimpostazione."),
    NL("Overschrijft wat de voorinstelling zegt, alleen voor deze rij. Laat het "
       "leeg om met de waarde van de voorinstelling te trainen."),
    RU("Переопределяет значение из пресета только для этой строки. Оставьте "
       "пустым, чтобы обучать со значением самого пресета."),
    TR("Yalnızca bu satır için hazır ayardaki değerin yerine geçer. Hazır ayarın "
       "kendi değeriyle eğitmek için boş bırakın."));
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
    EN("Runs go into this folder, each in its own timestamped subfolder. Leave "
       "it empty to write beside the dataset, which is what the trainer screen "
       "does."),
    JA("実行結果はこのフォルダーの中に、それぞれ日時のついたサブフォルダーとして"
       "入ります。空にすると、学習画面と同じくデータセットの隣に書き出します。"),
    ZH_HANS("每次运行都会放进这个文件夹，各自占一个带时间戳的子文件夹。留空则写在"
            "数据集旁边，和训练页面的做法一样。"),
    ZH_HANT("每次執行都會放進這個資料夾，各自佔一個帶時間戳的子資料夾。留空則寫在"
            "資料集旁邊，和訓練頁面的做法一樣。"),
    KO("실행 결과는 이 폴더 안에 각각 시각이 붙은 하위 폴더로 들어갑니다. 비워 "
       "두면 학습 화면과 마찬가지로 데이터셋 옆에 씁니다."),
    DE("Läufe kommen in diesen Ordner, jeder in einen eigenen Unterordner mit "
       "Zeitstempel. Leer lassen, um neben dem Datensatz zu schreiben -- so wie "
       "es der Trainingsbildschirm tut."),
    FR("Les exécutions vont dans ce dossier, chacune dans un sous-dossier "
       "horodaté. Laisser vide pour écrire à côté du jeu de données, comme le "
       "fait l'écran d'entraînement."),
    ES("Las ejecuciones van a esta carpeta, cada una en su subcarpeta con "
       "fecha y hora. Déjalo vacío para escribir junto al conjunto de datos, "
       "que es lo que hace la pantalla de entrenamiento."),
    PT("As execuções vão para esta pasta, cada uma em sua subpasta com data e "
       "hora. Deixe vazio para escrever ao lado do conjunto de dados, como faz "
       "a tela de treinamento."),
    IT("Le esecuzioni finiscono in questa cartella, ciascuna in una "
       "sottocartella con data e ora. Lascialo vuoto per scrivere accanto al "
       "set di dati, come fa la schermata di addestramento."),
    NL("Runs komen in deze map, elk in een eigen submap met tijdstempel. Laat "
       "het leeg om naast de dataset te schrijven, zoals het trainingsscherm "
       "doet."),
    RU("Запуски попадают в эту папку, каждый в свою подпапку с меткой времени. "
       "Оставьте пустым, чтобы писать рядом с набором данных, как делает экран "
       "обучения."),
    TR("Çalıştırmalar bu klasöre, her biri zaman damgalı kendi alt klasörüne "
       "gider. Veri kümesinin yanına yazmak için boş bırakın; eğitim ekranı da "
       "böyle yapar."));
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
    EN("Training"),      JA("学習中"),        ZH_HANS("训练中"),   ZH_HANT("訓練中"),
    KO("학습 중"),         DE("Training läuft"), FR("Entraînement"),
    ES("Entrenando"),    PT("Treinando"),    IT("In addestramento"),
    NL("Bezig met trainen"), RU("Обучается"), TR("Eğitiliyor"));
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
    EN("Job {0}: {1}"),
    JA("ジョブ {0}: {1}"),
    ZH_HANS("任务 {0}：{1}"),
    ZH_HANT("工作 {0}：{1}"),
    KO("작업 {0}: {1}"),
    DE("Auftrag {0}: {1}"),
    FR("Tâche {0} : {1}"),
    ES("Trabajo {0}: {1}"),
    PT("Trabalho {0}: {1}"),
    IT("Lavoro {0}: {1}"),
    NL("Taak {0}: {1}"),
    RU("Задание {0}: {1}"),
    TR("İş {0}: {1}"));
SS_MSG(batch_running_banner,
    EN("Batch job {0} of {1}"),
    JA("バッチのジョブ {0} / {1}"),
    ZH_HANS("批量任务 {0} / {1}"),
    ZH_HANT("批次工作 {0} / {1}"),
    KO("일괄 작업 {0} / {1}"),
    DE("Stapelauftrag {0} von {1}"),
    FR("Tâche du lot {0} sur {1}"),
    ES("Trabajo del lote {0} de {1}"),
    PT("Trabalho do lote {0} de {1}"),
    IT("Lavoro del batch {0} di {1}"),
    NL("Batchtaak {0} van {1}"),
    RU("Задание пакета {0} из {1}"),
    TR("Toplu iş {0} / {1}"));
SS_MSG(batch_running_dataset,
    EN("Training: {0}"),
    JA("学習中: {0}"),
    ZH_HANS("正在训练：{0}"),
    ZH_HANT("正在訓練：{0}"),
    KO("학습 중: {0}"),
    DE("Training: {0}"),
    FR("Entraînement : {0}"),
    ES("Entrenando: {0}"),
    PT("Treinando: {0}"),
    IT("Addestramento: {0}"),
    NL("Training: {0}"),
    RU("Обучение: {0}"),
    TR("Eğitiliyor: {0}"));
SS_MSG(batch_running_preset,
    EN("Preset: {0}"),
    JA("プリセット: {0}"),
    ZH_HANS("预设：{0}"),
    ZH_HANT("預設：{0}"),
    KO("프리셋: {0}"),
    DE("Voreinstellung: {0}"),
    FR("Préréglage : {0}"),
    ES("Ajuste: {0}"),
    PT("Predefinição: {0}"),
    IT("Preimpostazione: {0}"),
    NL("Voorinstelling: {0}"),
    RU("Пресет: {0}"),
    TR("Hazır ayar: {0}"));
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
    EN("Show training"),
    JA("学習の画面を表示"),
    ZH_HANS("显示训练画面"),
    ZH_HANT("顯示訓練畫面"),
    KO("학습 화면 보기"),
    DE("Training anzeigen"),
    FR("Afficher l'entraînement"),
    ES("Mostrar el entrenamiento"),
    PT("Mostrar o treinamento"),
    IT("Mostra l'addestramento"),
    NL("Training tonen"),
    RU("Показать обучение"),
    TR("Eğitimi göster"));
SS_MSG(batch_stop_after,
    EN("Stop after this job"),
    JA("このジョブの後で停止"),
    ZH_HANS("跑完这个任务后停止"),
    ZH_HANT("跑完這個工作後停止"),
    KO("이 작업 뒤에 중지"),
    DE("Nach diesem Auftrag anhalten"),
    FR("Arrêter après cette tâche"),
    ES("Parar después de este trabajo"),
    PT("Parar depois deste trabalho"),
    IT("Fermarsi dopo questo lavoro"),
    NL("Stoppen na deze taak"),
    RU("Остановиться после этого задания"),
    TR("Bu işten sonra dur"));
SS_MSG(batch_stop_after_help,
    EN("Let the job that is running finish and save, then stop instead of "
       "starting the next one."),
    JA("実行中のジョブは最後まで走らせて保存し、次を始めずにそこで止めます。"),
    ZH_HANS("让正在跑的任务跑完并保存，然后就此停下，不再开始下一个。"),
    ZH_HANT("讓正在跑的工作跑完並儲存，然後就此停下，不再開始下一個。"),
    KO("실행 중인 작업은 끝까지 돌려 저장한 뒤, 다음 작업을 시작하지 않고 "
       "멈춥니다."),
    DE("Den laufenden Auftrag zu Ende bringen und speichern lassen, dann "
       "anhalten, statt den nächsten zu starten."),
    FR("Laisser la tâche en cours finir et enregistrer, puis s'arrêter au lieu "
       "de lancer la suivante."),
    ES("Dejar que el trabajo en curso termine y guarde, y luego parar en vez de "
       "arrancar el siguiente."),
    PT("Deixar o trabalho em andamento terminar e salvar, e então parar em vez "
       "de começar o próximo."),
    IT("Lasciare che il lavoro in corso finisca e salvi, poi fermarsi invece di "
       "avviare il successivo."),
    NL("De lopende taak laten afmaken en opslaan, en dan stoppen in plaats van "
       "de volgende te starten."),
    RU("Дать текущему заданию доработать и сохраниться, а затем остановиться, "
       "не запуская следующее."),
    TR("Çalışan işin bitip kaydetmesini bekler, sonra bir sonrakini başlatmak "
       "yerine durur."));
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
    EN("Cut the running job short -- it still saves a checkpoint -- and stop "
       "the batch."),
    JA("実行中のジョブを途中で打ち切り（チェックポイントは保存されます）、"
       "バッチを止めます。"),
    ZH_HANS("把正在跑的任务提前结束（仍会保存一个检查点），并停止整批。"),
    ZH_HANT("把正在跑的工作提前結束（仍會儲存一個檢查點），並停止整批。"),
    KO("실행 중인 작업을 중간에 끊고(체크포인트는 저장합니다) 일괄 실행을 "
       "멈춥니다."),
    DE("Den laufenden Auftrag abkürzen -- ein Checkpoint wird trotzdem "
       "gespeichert -- und den Stapel anhalten."),
    FR("Interrompre la tâche en cours -- un point de reprise est tout de même "
       "enregistré -- et arrêter le lot."),
    ES("Cortar el trabajo en curso -- aun así guarda un punto de control -- y "
       "parar el lote."),
    PT("Encerrar antes o trabalho em andamento -- um ponto de verificação ainda "
       "é salvo -- e parar o lote."),
    IT("Interrompere il lavoro in corso -- un checkpoint viene comunque salvato "
       "-- e fermare il batch."),
    NL("De lopende taak afbreken -- er wordt nog wel een controlepunt "
       "opgeslagen -- en de batch stoppen."),
    RU("Оборвать текущее задание -- контрольная точка всё равно сохранится -- и "
       "остановить пакет."),
    TR("Çalışan işi yarıda keser -- yine de bir denetim noktası kaydedilir -- "
       "ve toplu işi durdurur."));
SS_MSG(batch_stopping,
    EN("Stopping after this job."),
    JA("このジョブの後で停止します。"),
    ZH_HANS("将在这个任务之后停止。"),
    ZH_HANT("將在這個工作之後停止。"),
    KO("이 작업 뒤에 멈춥니다."),
    DE("Wird nach diesem Auftrag angehalten."),
    FR("Arrêt après cette tâche."),
    ES("Se parará después de este trabajo."),
    PT("Vai parar depois deste trabalho."),
    IT("Ci si fermerà dopo questo lavoro."),
    NL("Stopt na deze taak."),
    RU("Остановка после этого задания."),
    TR("Bu işten sonra durulacak."));

SS_MSG(batch_log_started,
    EN("Batch started. Jobs: {0}"),
    JA("バッチを開始しました。ジョブ数: {0}"),
    ZH_HANS("批量训练已开始。任务数：{0}"),
    ZH_HANT("批次訓練已開始。工作數：{0}"),
    KO("일괄 실행을 시작했습니다. 작업 수: {0}"),
    DE("Stapel gestartet. Aufträge: {0}"),
    FR("Lot démarré. Tâches : {0}"),
    ES("Lote iniciado. Trabajos: {0}"),
    PT("Lote iniciado. Trabalhos: {0}"),
    IT("Batch avviato. Lavori: {0}"),
    NL("Batch gestart. Taken: {0}"),
    RU("Пакет запущен. Заданий: {0}"),
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
    EN("Batch job {0} finished, written to {1}"),
    JA("バッチのジョブ {0} が完了し、{1} に書き出しました"),
    ZH_HANS("批量任务 {0} 已完成，写入 {1}"),
    ZH_HANT("批次工作 {0} 已完成，寫入 {1}"),
    KO("일괄 작업 {0}을(를) 마치고 {1}에 썼습니다"),
    DE("Stapelauftrag {0} beendet, geschrieben nach {1}"),
    FR("Tâche du lot {0} terminée, écrite dans {1}"),
    ES("Trabajo del lote {0} terminado, escrito en {1}"),
    PT("Trabalho do lote {0} concluído, escrito em {1}"),
    IT("Lavoro del batch {0} terminato, scritto in {1}"),
    NL("Batchtaak {0} klaar, weggeschreven naar {1}"),
    RU("Задание пакета {0} завершено, записано в {1}"),
    TR("Toplu iş {0} bitti, {1} konumuna yazıldı"));
SS_MSG(batch_log_job_failed,
    EN("Batch job {0} failed: {1}"),
    JA("バッチのジョブ {0} が失敗しました: {1}"),
    ZH_HANS("批量任务 {0} 失败：{1}"),
    ZH_HANT("批次工作 {0} 失敗：{1}"),
    KO("일괄 작업 {0}이(가) 실패했습니다: {1}"),
    DE("Stapelauftrag {0} fehlgeschlagen: {1}"),
    FR("Échec de la tâche du lot {0} : {1}"),
    ES("El trabajo del lote {0} ha fallado: {1}"),
    PT("O trabalho do lote {0} falhou: {1}"),
    IT("Il lavoro del batch {0} non è riuscito: {1}"),
    NL("Batchtaak {0} is mislukt: {1}"),
    RU("Задание пакета {0} завершилось сбоем: {1}"),
    TR("Toplu iş {0} başarısız oldu: {1}"));
SS_MSG(batch_log_job_stopped,
    EN("Batch job {0} was stopped."),
    JA("バッチのジョブ {0} は停止されました。"),
    ZH_HANS("批量任务 {0} 已被停止。"),
    ZH_HANT("批次工作 {0} 已被停止。"),
    KO("일괄 작업 {0}이(가) 중지되었습니다."),
    DE("Stapelauftrag {0} wurde angehalten."),
    FR("La tâche du lot {0} a été arrêtée."),
    ES("El trabajo del lote {0} se ha detenido."),
    PT("O trabalho do lote {0} foi interrompido."),
    IT("Il lavoro del batch {0} è stato interrotto."),
    NL("Batchtaak {0} is gestopt."),
    RU("Задание пакета {0} остановлено."),
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
    EN("Turn a trained model into a triangle surface you can open in Blender, "
       "3D print, or drop into a game engine. Uses the training photos when "
       "they are still on disk, which makes the surface much cleaner."),
    JA("学習済みモデルを三角形の面に変換します。Blenderで開く、3Dプリントする、"
       "ゲームエンジンに読み込む、といった使い方ができます。学習に使った写真が"
       "残っていればそれも使い、面がずっときれいになります。"),
    ZH_HANS("把训练好的模型变成三角面片，可以在 Blender 里打开、3D 打印，或者放进"
            "游戏引擎。如果训练用的照片还在，也会一起使用，表面会干净很多。"),
    ZH_HANT("把訓練好的模型變成三角面片，可以在 Blender 裡開啟、3D 列印，或者放進"
            "遊戲引擎。如果訓練用的相片還在，也會一起使用，表面會乾淨很多。"),
    KO("학습한 모델을 삼각형 표면으로 바꿉니다. Blender에서 열거나 3D 프린트하거나 "
       "게임 엔진에 넣을 수 있습니다. 학습에 쓴 사진이 남아 있으면 함께 사용해서 "
       "표면이 훨씬 깨끗해집니다."),
    DE("Macht aus einem trainierten Modell eine Dreiecksfläche, die sich in "
       "Blender öffnen, 3D-drucken oder in eine Spiel-Engine laden lässt. "
       "Nutzt die Trainingsfotos, wenn sie noch da sind -- das macht die "
       "Oberfläche deutlich sauberer."),
    FR("Transforme un modèle entraîné en une surface de triangles, à ouvrir "
       "dans Blender, à imprimer en 3D ou à charger dans un moteur de jeu. "
       "Utilise les photos d'entraînement si elles sont encore là, ce qui rend "
       "la surface bien plus propre."),
    ES("Convierte un modelo entrenado en una superficie de triángulos que "
       "puedes abrir en Blender, imprimir en 3D o cargar en un motor de "
       "juego. Usa las fotos de entrenamiento si siguen ahí, lo que deja la "
       "superficie mucho más limpia."),
    PT("Transforma um modelo treinado em uma superfície de triângulos que você "
       "pode abrir no Blender, imprimir em 3D ou carregar em um motor de "
       "jogo. Usa as fotos de treinamento se ainda estiverem lá, o que deixa a "
       "superfície bem mais limpa."),
    IT("Trasforma un modello addestrato in una superficie di triangoli da "
       "aprire in Blender, stampare in 3D o caricare in un motore di gioco. "
       "Usa le foto di addestramento se ci sono ancora, e la superficie viene "
       "molto più pulita."),
    NL("Maakt van een getraind model een driehoeksoppervlak dat je in Blender "
       "kunt openen, 3D kunt printen of in een game-engine kunt laden. Gebruikt "
       "de trainingsfoto's als die er nog zijn; dat maakt het oppervlak veel "
       "schoner."),
    RU("Превращает обученную модель в треугольную поверхность: её можно открыть "
       "в Blender, напечатать на 3D-принтере или загрузить в игровой движок. "
       "Если фотографии обучения ещё на диске, они тоже используются, и "
       "поверхность выходит гораздо чище."),
    TR("Eğitilmiş bir modeli, Blender'da açabileceğiniz, 3B yazdırabileceğiniz "
       "ya da bir oyun motoruna alabileceğiniz üçgen yüzeye dönüştürür. Eğitim "
       "fotoğrafları hâlâ diskteyse onları da kullanır ve yüzey çok daha temiz "
       "çıkar."));

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
    EN("How each Gaussian is drawn. 3DGS is what most models are trained as; "
       "Mip antialiases; 3DGUT evaluates the Gaussian along each pixel's ray "
       "and is the honest one for fisheye and 360 views."),
    JA("ガウシアンの描き方です。3DGSはほとんどのモデルの学習方法、Mipは"
       "アンチエイリアスあり、3DGUTはピクセルごとの光線に沿って評価するもので、"
       "魚眼や360度の表示ではこれが正確です。"),
    ZH_HANS("每个高斯怎么画。3DGS 是大多数模型的训练方式；Mip 会抗锯齿；"
            "3DGUT 沿每个像素的光线求值，鱼眼和 360 度视图下最准确。"),
    ZH_HANT("每個高斯怎麼畫。3DGS 是大多數模型的訓練方式；Mip 會抗鋸齒；"
            "3DGUT 沿每個像素的光線求值，魚眼和 360 度檢視下最準確。"),
    KO("가우시안을 어떻게 그릴지입니다. 3DGS는 대부분의 모델이 학습된 방식, "
       "Mip은 계단 현상을 줄이고, 3DGUT는 픽셀마다 광선을 따라 계산해서 어안과 "
       "360도 화면에서 가장 정확합니다."),
    DE("Wie jede Gauß-Verteilung gezeichnet wird. 3DGS ist, womit die meisten "
       "Modelle trainiert werden; Mip glättet Kanten; 3DGUT wertet entlang des "
       "Strahls jedes Pixels aus und ist bei Fisheye- und 360-Grad-Ansichten "
       "das ehrliche Verfahren."),
    FR("Comment chaque gaussienne est dessinée. 3DGS est ce avec quoi la "
       "plupart des modèles sont entraînés ; Mip lisse les bords ; 3DGUT "
       "évalue le long du rayon de chaque pixel et c'est le procédé honnête "
       "en fisheye et en 360 degrés."),
    ES("Cómo se dibuja cada gaussiana. 3DGS es con lo que se entrena la "
       "mayoría de los modelos; Mip suaviza los bordes; 3DGUT evalúa a lo "
       "largo del rayo de cada píxel y es el honesto en ojo de pez y 360 "
       "grados."),
    PT("Como cada gaussiana é desenhada. 3DGS é com o que a maioria dos "
       "modelos é treinada; Mip suaviza as bordas; 3DGUT avalia ao longo do "
       "raio de cada pixel e é o honesto em olho de peixe e 360 graus."),
    IT("Come viene disegnata ogni gaussiana. 3DGS è ciò con cui viene "
       "addestrata la maggior parte dei modelli; Mip attenua i bordi; 3DGUT "
       "valuta lungo il raggio di ogni pixel ed è quello onesto in fisheye e "
       "a 360 gradi."),
    NL("Hoe elke Gaussiaan wordt getekend. 3DGS is waarmee de meeste modellen "
       "getraind zijn; Mip haalt kartelranden weg; 3DGUT rekent langs de "
       "straal van elke pixel en is de eerlijke keuze bij fisheye en 360 "
       "graden."),
    RU("Как рисуется каждая гауссиана. 3DGS -- то, на чём обучено большинство "
       "моделей; Mip сглаживает края; 3DGUT считает вдоль луча каждого "
       "пикселя и честнее всего работает на «рыбьем глазе» и в 360 градусах."),
    TR("Her gauss'un nasıl çizileceği. 3DGS çoğu modelin eğitildiği yöntem; "
       "Mip kenar pürüzlerini giderir; 3DGUT her pikselin ışını boyunca hesap "
       "yapar ve balıkgözü ile 360 derece görünümlerde dürüst olanıdır."));

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
    EN("The curve the model's values leave through on their way to the screen. "
       "`aces`, `filmic` and `uncharted2` roll the highlights off instead of "
       "clipping them. Set it to what the run was trained with "
       "(--splat-color-transfer)."),
    JA("モデルの値が画面へ出ていくときに通るカーブです。aces・filmic・"
       "uncharted2 はハイライトを切り捨てずになだらかに丸めます。学習時の設定"
       "（--splat-color-transfer）に合わせてください。"),
    ZH_HANS("模型数值送到屏幕时经过的曲线。aces、filmic、uncharted2 会把高光平滑"
            "压下来而不是直接截断。设成训练时用的那个"
            "（--splat-color-transfer）。"),
    ZH_HANT("模型數值送到螢幕時經過的曲線。aces、filmic、uncharted2 會把高光平滑"
            "壓下來而不是直接截斷。設成訓練時用的那個"
            "（--splat-color-transfer）。"),
    KO("모델 값이 화면으로 나갈 때 지나는 곡선입니다. aces, filmic, uncharted2는 "
       "밝은 부분을 잘라내지 않고 완만하게 눌러 줍니다. 학습할 때 쓴 것"
       "(--splat-color-transfer)으로 맞추세요."),
    DE("Die Kurve, über die die Werte des Modells zum Bildschirm gehen. `aces`, "
       "`filmic` und `uncharted2` rollen die Lichter ab, statt sie "
       "abzuschneiden. Auf den Trainingswert einstellen "
       "(--splat-color-transfer)."),
    FR("La courbe par laquelle les valeurs du modèle sortent vers l'écran. "
       "« aces », « filmic » et « uncharted2 » adoucissent les hautes lumières "
       "au lieu de les écrêter. Réglez-le sur celui de l'entraînement "
       "(--splat-color-transfer)."),
    ES("La curva por la que los valores del modelo salen hacia la pantalla. "
       "«aces», «filmic» y «uncharted2» suavizan las altas luces en vez de "
       "recortarlas. Ponlo como se entrenó (--splat-color-transfer)."),
    PT("A curva por que os valores do modelo saem para a tela. «aces», «filmic» "
       "e «uncharted2» suavizam as altas luzes em vez de as cortar. Coloque como "
       "foi treinado (--splat-color-transfer)."),
    IT("La curva da cui i valori del modello escono verso lo schermo. «aces», "
       "«filmic» e «uncharted2» addolciscono le alte luci invece di troncarle. "
       "Impostalo come l'addestramento (--splat-color-transfer)."),
    NL("De kromme waarlangs de waarden van het model naar het scherm gaan. "
       "`aces`, `filmic` en `uncharted2` laten de hoge lichten aflopen in plaats "
       "van ze af te kappen. Zet het op waarmee de run getraind is "
       "(--splat-color-transfer)."),
    RU("Кривая, через которую значения модели уходят на экран. «aces», «filmic» "
       "и «uncharted2» плавно сводят света вместо обрезки. Поставьте то, с "
       "которым шло обучение (--splat-color-transfer)."),
    TR("Modelin değerlerinin ekrana çıkarken geçtiği eğri. `aces`, `filmic` ve "
       "`uncharted2` parlak bölgeleri kırpmak yerine yumuşatarak indirir. "
       "Eğitimde ne kullanıldıysa onu seçin (--splat-color-transfer)."));

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
    EN("The surface is carved from what the cameras actually saw, which "
       "removes interior fog and gives much better color. Turn this off to "
       "mesh from the Gaussians alone."),
    JA("カメラが実際に見たものから面を削り出します。内部のもやが消え、色も"
       "ずっと良くなります。オフにするとガウシアンだけからメッシュを作ります。"),
    ZH_HANS("用相机真正看到的内容来雕出表面，可以去掉内部的雾，颜色也好很多。"
            "关掉的话就只用高斯本身生成网格。"),
    ZH_HANT("用相機真正看到的內容來雕出表面，可以去掉內部的霧，顏色也好很多。"
            "關掉的話就只用高斯本身產生網格。"),
    KO("카메라가 실제로 본 것에서 표면을 깎아냅니다. 내부의 안개가 사라지고 색도 "
       "훨씬 좋아집니다. 끄면 가우시안만으로 메시를 만듭니다."),
    DE("Die Oberfläche wird aus dem geschnitten, was die Kameras wirklich "
       "gesehen haben: kein Nebel im Inneren und deutlich bessere Farben. "
       "Ausschalten, um nur aus den Gauß-Verteilungen zu vernetzen."),
    FR("La surface est taillée dans ce que les caméras ont vraiment vu : plus "
       "de brume à l'intérieur et de bien meilleures couleurs. Désactivez pour "
       "mailler à partir des seules gaussiennes."),
    ES("La superficie se talla a partir de lo que las cámaras vieron de "
       "verdad: quita la niebla interior y da mucho mejor color. Desactívalo "
       "para mallar solo con las gaussianas."),
    PT("A superfície é esculpida a partir do que as câmeras realmente viram: "
       "tira a névoa interna e dá cores bem melhores. Desligue para gerar a "
       "malha só com as gaussianas."),
    IT("La superficie viene scavata da ciò che le fotocamere hanno visto "
       "davvero: niente nebbia interna e colori molto migliori. Disattiva per "
       "creare la mesh dalle sole gaussiane."),
    NL("Het oppervlak wordt uitgesneden uit wat de camera's echt zagen: geen "
       "mist binnenin en veel betere kleuren. Zet dit uit om alleen uit de "
       "Gaussianen een mesh te maken."),
    RU("Поверхность вырезается по тому, что действительно видели камеры: "
       "исчезает внутренний туман и цвет получается заметно лучше. Выключите, "
       "чтобы строить меш только по гауссианам."),
    TR("Yüzey, kameraların gerçekten gördüğünden oyulur: içerideki sis gider "
       "ve renk çok daha iyi olur. Yalnızca gauss'lardan ağ oluşturmak için "
       "kapatın."));

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
    EN("0 uses every photo, which is best but can be slow on a dataset with "
       "many of them. A smaller positive number picks a well-spread subset."),
    JA("0 ならすべての写真を使います。いちばん良い結果になりますが、写真の"
       "多いデータセットでは時間がかかります。正の小さい数にすると満遍なく"
       "散らばった一部だけを使います。"),
    ZH_HANS("0 表示用上全部照片，效果最好，但照片很多的数据集会比较慢。"
            "填一个较小的正数会挑一批分布均匀的照片。"),
    ZH_HANT("0 表示用上全部相片，效果最好，但相片很多的資料集會比較慢。"
            "填一個較小的正數會挑一批分布均勻的相片。"),
    KO("0이면 사진을 모두 씁니다. 결과는 가장 좋지만 사진이 많은 데이터셋에서는 "
       "느릴 수 있습니다. 작은 양수를 넣으면 고르게 퍼진 일부만 고릅니다."),
    DE("0 nutzt jedes Foto -- das beste Ergebnis, bei einem Datensatz mit "
       "vielen Fotos aber langsam. Eine kleinere positive Zahl wählt eine gut "
       "verteilte Teilmenge."),
    FR("0 utilise toutes les photos : c'est le meilleur résultat, mais cela "
       "peut être lent sur un jeu de données qui en compte beaucoup. Un "
       "nombre positif plus petit choisit un sous-ensemble bien réparti."),
    ES("0 usa todas las fotos: es lo mejor, pero puede ser lento en un "
       "conjunto con muchas. Un número positivo menor elige un subconjunto "
       "bien repartido."),
    PT("0 usa todas as fotos: é o melhor, mas pode ser lento num conjunto com "
       "muitas. Um número positivo menor escolhe um subconjunto bem "
       "espalhado."),
    IT("0 usa tutte le foto: è il risultato migliore, ma può essere lento su "
       "un set di dati che ne contiene molte. Un numero positivo più piccolo "
       "sceglie un sottoinsieme ben distribuito."),
    NL("0 gebruikt elke foto: dat is het best, maar bij een dataset met veel "
       "foto's kan het traag zijn. Een kleiner positief getal kiest een goed "
       "verspreide selectie."),
    RU("0 берёт все фотографии — результат лучший, но на наборе с большим их "
       "числом это медленно. Меньшее положительное число выбирает равномерно "
       "разбросанное подмножество."),
    TR("0 tüm fotoğrafları kullanır; en iyi sonuç budur ama fotoğrafı çok "
       "olan bir veri kümesinde yavaş olabilir. Daha küçük bir pozitif sayı "
       "iyi dağılmış bir alt küme seçer."));

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


// ===========================================================================
// Photograph vs render (src/app/gui/ImageCompare.h)
// ===========================================================================

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
    EN("Previous and next training image. The left and right arrow keys do the "
       "same; Page Up and Page Down move ten at a time, Home and End go to the "
       "first and the last."),
    JA("前後の学習画像に移動します。左右の矢印キーでも同じことができ、Page Up と "
       "Page Down は10枚ずつ、Home と End は最初と最後に移動します。"),
    ZH_HANS("切换到上一张或下一张训练图像。左右方向键作用相同；Page Up 和 "
            "Page Down 一次跳十张，Home 和 End 跳到第一张和最后一张。"),
    ZH_HANT("切換到上一張或下一張訓練影像。左右方向鍵作用相同；Page Up 和 "
            "Page Down 一次跳十張，Home 和 End 跳到第一張和最後一張。"),
    KO("이전·다음 학습 이미지로 이동합니다. 좌우 화살표 키도 같은 일을 하고, "
       "Page Up과 Page Down은 열 장씩, Home과 End는 처음과 끝으로 갑니다."),
    DE("Vorheriges und nächstes Trainingsbild. Die Pfeiltasten links und rechts "
       "tun dasselbe; Bild auf und Bild ab springen zehn weiter, Pos1 und Ende "
       "an den Anfang und ans Ende."),
    FR("Image d'entraînement précédente et suivante. Les flèches gauche et "
       "droite font la même chose ; Page précédente et Page suivante avancent "
       "de dix, Origine et Fin vont au début et à la fin."),
    ES("Imagen de entrenamiento anterior y siguiente. Las flechas izquierda y "
       "derecha hacen lo mismo; Re Pág y Av Pág saltan de diez en diez, Inicio "
       "y Fin van al principio y al final."),
    PT("Imagem de treinamento anterior e seguinte. As setas para a esquerda e "
       "para a direita fazem o mesmo; Page Up e Page Down saltam de dez em dez, "
       "Home e End vão ao início e ao fim."),
    IT("Immagine di addestramento precedente e successiva. Le frecce sinistra e "
       "destra fanno lo stesso; Pag su e Pag giù saltano di dieci, Inizio e "
       "Fine vanno al principio e alla fine."),
    NL("Vorig en volgend trainingsbeeld. De pijltjestoetsen links en rechts "
       "doen hetzelfde; Page Up en Page Down springen tien verder, Home en End "
       "naar het begin en het einde."),
    RU("Предыдущий и следующий обучающий снимок. Стрелки влево и вправо делают "
       "то же самое; Page Up и Page Down перемещают на десять, Home и End — в "
       "начало и в конец."),
    TR("Önceki ve sonraki eğitim görüntüsü. Sol ve sağ ok tuşları da aynı işi "
       "yapar; Page Up ve Page Down onar atlar, Home ve End başa ve sona "
       "gider."));

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
    EN("How the part of the image the mask excludes is drawn. Training does not "
       "look at it at all, so both sides mark it the same way -- and the score "
       "below counts only what is left."),
    JA("マスクで除外された部分の描き方です。学習はその部分をまったく見ないので、"
       "両側とも同じ印を付けます。下のスコアも残った部分だけで計算します。"),
    ZH_HANS("遮罩排除的部分怎么显示。训练完全不看这部分，所以两边用同样的方式"
            "标出；下面的分数也只统计剩下的部分。"),
    ZH_HANT("遮罩排除的部分怎麼顯示。訓練完全不看這部分，所以兩邊用同樣的方式"
            "標出；下面的分數也只統計剩下的部分。"),
    KO("마스크가 제외한 부분을 어떻게 그릴지 정합니다. 학습은 그 부분을 전혀 "
       "보지 않으므로 양쪽에 같은 표시를 하고, 아래 점수도 남은 부분만으로 "
       "계산합니다."),
    DE("Wie der von der Maske ausgeschlossene Teil gezeichnet wird. Das "
       "Training sieht ihn gar nicht, also markieren ihn beide Seiten gleich -- "
       "und der Wert darunter zählt nur den Rest."),
    FR("Comment est dessinée la partie que le masque exclut. L'entraînement ne "
       "la voit pas du tout, donc les deux côtés la marquent de la même façon, "
       "et le score ci-dessous ne compte que le reste."),
    ES("Cómo se dibuja la parte que la máscara excluye. El entrenamiento no la "
       "ve en absoluto, así que ambos lados la marcan igual, y la puntuación de "
       "abajo solo cuenta el resto."),
    PT("Como é desenhada a parte que a máscara exclui. O treinamento não a vê, "
       "então os dois lados a marcam do mesmo jeito, e a pontuação abaixo conta "
       "apenas o resto."),
    IT("Come viene disegnata la parte che la maschera esclude. L'addestramento "
       "non la vede affatto, quindi entrambi i lati la segnano allo stesso modo "
       "e il punteggio sotto conta solo il resto."),
    NL("Hoe het deel dat het masker uitsluit wordt getekend. Het trainen ziet "
       "dat deel helemaal niet, dus beide kanten markeren het gelijk, en de "
       "score eronder telt alleen de rest."),
    RU("Как рисуется часть, исключённая маской. Обучение её вовсе не видит, "
       "поэтому обе стороны помечают её одинаково, а оценка ниже считает только "
       "остальное."),
    TR("Maskenin dışladığı bölümün nasıl çizileceği. Eğitim orayı hiç görmez, "
       "bu yüzden iki taraf da aynı biçimde işaretler ve aşağıdaki puan "
       "yalnızca kalanı sayar."));

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

SS_MSG(compare_live_help,
    EN("Re-render the shown image while training, so the comparison keeps up "
       "with the optimization."),
    JA("学習中も表示中の画像を描き直し、比較が最適化に追いつくようにします。"),
    ZH_HANS("训练时不断重新渲染当前图像，让对比跟上优化过程。"),
    ZH_HANT("訓練時不斷重新算繪目前影像，讓比較跟上最佳化過程。"),
    KO("학습 중에도 보고 있는 이미지를 다시 렌더링해서 비교가 최적화를 "
       "따라가게 합니다."),
    DE("Das gezeigte Bild während des Trainings neu rendern, damit der "
       "Vergleich der Optimierung folgt."),
    FR("Refaire le rendu de l'image affichée pendant l'entraînement, pour que "
       "la comparaison suive l'optimisation."),
    ES("Volver a renderizar la imagen mostrada durante el entrenamiento, para "
       "que la comparación siga a la optimización."),
    PT("Renderizar de novo a imagem mostrada durante o treinamento, para que a "
       "comparação acompanhe a otimização."),
    IT("Rifare il render dell'immagine mostrata durante l'addestramento, così "
       "il confronto segue l'ottimizzazione."),
    NL("Het getoonde beeld tijdens het trainen opnieuw renderen, zodat de "
       "vergelijking de optimalisatie volgt."),
    RU("Перерисовывать показанный снимок во время обучения, чтобы сравнение "
       "поспевало за оптимизацией."),
    TR("Eğitim sürerken gösterilen görüntüyü yeniden oluştur, böylece "
       "karşılaştırma eniyilemeye ayak uydurur."));

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
    EN("The error map this run hands to the backward pass, where it becomes "
       "each splat's need-more-detail score. It is multi-scale and "
       "post-processed exactly as the detail error measure sets it, so the "
       "warm colours are where new splats will go."),
    JA("この学習が逆伝播に渡している誤差マップそのものです。各スプラットの"
       "「もっと細かくすべき」スコアはここから積み上がります。詳細の誤差の測り方"
       "の設定どおりに多重解像度と後処理を通してあるので、暖色の場所に新しい"
       "スプラットが足されます。"),
    ZH_HANS("本次训练真正送入反向传播的误差图，每个泼溅的“需要更多细节”评分就是"
            "从这里累积的。它按细节误差度量的设置做过多尺度与后处理，所以暖色的"
            "地方就是新泼溅将要落下的位置。"),
    ZH_HANT("本次訓練真正送入反向傳播的誤差圖，每個潑濺的「需要更多細節」評分就是"
            "從這裡累積的。它按細節誤差度量的設定做過多尺度與後處理，所以暖色的"
            "地方就是新潑濺將要落下的位置。"),
    KO("이 학습이 실제로 역전파에 넘기는 오차 맵입니다. 각 스플랫의 "
       "「더 자세히 그려야 한다」 점수가 여기서 쌓입니다. 디테일 오차 측정 방식이 "
       "정한 그대로 다중 해상도와 후처리를 거치므로, 따뜻한 색이 새 스플랫이 "
       "놓일 자리입니다."),
    DE("Genau die Fehlerkarte, die dieser Lauf an den Rückwärtsdurchlauf gibt "
       "und aus der jeder Splat seinen Detailbedarf bezieht. Sie ist "
       "mehrskalig und nachbearbeitet, genau wie das Detailfehlermaß es "
       "einstellt -- warme Farben zeigen, wo neue Splats hinkommen."),
    FR("La carte d'erreur que cette session transmet réellement à la passe "
       "arrière, d'où chaque splat tire son score de besoin de détail. Elle "
       "est multi-échelle et post-traitée exactement comme la mesure d'erreur "
       "de détail le règle : les couleurs chaudes montrent où iront les "
       "nouveaux splats."),
    ES("El mapa de error que esta ejecución entrega de verdad a la pasada "
       "hacia atrás, del que cada splat saca su puntuación de necesidad de "
       "detalle. Es multiescala y está posprocesado tal como lo fija la "
       "medida de error de detalle: los colores cálidos indican dónde irán "
       "los splats nuevos."),
    PT("O mapa de erro que esta execução entrega de facto à passagem para "
       "trás, de onde cada splat tira a sua pontuação de necessidade de "
       "detalhe. É multiescala e pós-processado exatamente como a medida de "
       "erro de detalhe define: as cores quentes mostram onde vão os splats "
       "novos."),
    IT("La mappa dell'errore che questa esecuzione passa davvero al passaggio "
       "all'indietro, da cui ogni splat ricava il punteggio di bisogno di "
       "dettaglio. È multiscala e post-elaborata esattamente come la imposta "
       "la misura dell'errore di dettaglio: i colori caldi mostrano dove "
       "andranno gli splat nuovi."),
    NL("Precies de foutkaart die deze run aan de terugwaartse stap geeft en "
       "waaruit elke splat zijn detailbehoefte haalt. Ze is multischaal en "
       "nabewerkt zoals de detailfoutmaat het instelt -- warme kleuren laten "
       "zien waar nieuwe splats komen."),
    RU("Та самая карта ошибки, которую этот запуск передаёт в обратный проход "
       "и из которой каждый сплат берёт свою потребность в детализации. Она "
       "многомасштабная и обработана ровно так, как задаёт мера ошибки "
       "детализации: тёплые цвета показывают, где появятся новые сплаты."),
    TR("Bu çalışmanın geri geçişe gerçekten verdiği hata haritası; her "
       "splat'ın daha fazla ayrıntı gereksinimi puanı buradan birikir. Çok "
       "ölçeklidir ve ayrıntı hata ölçüsünün belirlediği gibi son işlemden "
       "geçmiştir: sıcak renkler yeni splat'ların nereye gideceğini "
       "gösterir."));

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
    EN("A row for the depth the run supervises against, beside the depth the "
       "model renders. Each is coloured over its own range -- the depth term "
       "correlates them and ignores scale, so only the shape is comparable."),
    JA("学習が教師にしている深度と、モデルが描いた深度を並べた段を出します。"
       "色はそれぞれの範囲で付けます。深度項は両者の相関を取ってスケールを無視"
       "するので、比べられるのは形だけです。"),
    ZH_HANS("增加一行：训练用作监督的深度，与模型渲染出的深度并排。两者各按自身"
            "范围上色——深度项计算相关性、忽略尺度，所以能比较的只有形状。"),
    ZH_HANT("增加一列：訓練用作監督的深度，與模型算繪出的深度並排。兩者各按自身"
            "範圍上色——深度項計算相關性、忽略尺度，所以能比較的只有形狀。"),
    KO("학습이 교사로 쓰는 깊이와 모델이 그린 깊이를 나란히 놓은 줄을 켭니다. "
       "색은 각자의 범위로 칠합니다. 깊이 항은 둘의 상관을 보고 배율을 무시하므로 "
       "비교할 수 있는 것은 형태뿐입니다."),
    DE("Eine Zeile für die Tiefe, gegen die trainiert wird, neben der Tiefe, "
       "die das Modell rendert. Jede ist über ihren eigenen Bereich eingefärbt "
       "-- der Tiefenterm korreliert sie und ignoriert den Maßstab, also ist "
       "nur die Form vergleichbar."),
    FR("Une rangée pour la profondeur qui sert de référence, à côté de celle "
       "que le modèle rend. Chacune est colorée sur sa propre plage : le terme "
       "de profondeur les corrèle et ignore l'échelle, seule la forme est donc "
       "comparable."),
    ES("Una fila para la profundidad con la que se supervisa, junto a la que "
       "renderiza el modelo. Cada una se colorea sobre su propio rango: el "
       "término de profundidad las correlaciona e ignora la escala, así que "
       "solo la forma es comparable."),
    PT("Uma linha para a profundidade que serve de referência, ao lado da que o "
       "modelo renderiza. Cada uma é colorida sobre o seu próprio intervalo: o "
       "termo de profundidade correlaciona-as e ignora a escala, por isso só a "
       "forma é comparável."),
    IT("Una riga per la profondità usata come riferimento, accanto a quella che "
       "il modello rende. Ognuna è colorata sul proprio intervallo: il termine "
       "di profondità le correla e ignora la scala, quindi è confrontabile solo "
       "la forma."),
    NL("Een rij voor de diepte waartegen de run traint, naast de diepte die het "
       "model rendert. Elk is over zijn eigen bereik ingekleurd -- de "
       "diepteterm correleert ze en negeert schaal, dus alleen de vorm is "
       "vergelijkbaar."),
    RU("Ряд для глубины, по которой идёт обучение, рядом с глубиной, которую "
       "рисует модель. Каждая раскрашена по своему диапазону: член глубины "
       "коррелирует их и не смотрит на масштаб, так что сравнима только форма."),
    TR("Eğitimin denetim için kullandığı derinlik ile modelin görüntülediği "
       "derinliği yan yana koyan bir satır. Her biri kendi aralığına göre "
       "renklendirilir; derinlik terimi ikisini ilişkilendirir ve ölçeği yok "
       "sayar, dolayısıyla yalnızca biçim karşılaştırılabilir."));

SS_MSG(compare_show_normal,
    EN("Normals"),        JA("法線"),          ZH_HANS("法线"),     ZH_HANT("法線"),
    KO("법선"),            DE("Normalen"),     FR("Normales"),     ES("Normales"),
    PT("Normais"),        IT("Normali"),      NL("Normalen"),     RU("Нормали"),
    TR("Normaller"));

SS_MSG(compare_show_normal_help,
    EN("A row for the normals the run supervises against, beside the normals "
       "the model has. No primitive renders normals, so the right-hand pane is "
       "the one derived from the rendered depth -- which is what the normal "
       "term compares."),
    JA("学習が教師にしている法線と、モデルの法線を並べた段を出します。法線を直接"
       "描くプリミティブは無いので、右側はレンダリングした深度から求めた法線です。"
       "法線項が比べているのもそれです。"),
    ZH_HANS("增加一行：训练用作监督的法线，与模型的法线并排。没有任何基元直接渲染"
            "法线，所以右侧是从渲染深度求出的法线——法线项比较的也正是它。"),
    ZH_HANT("增加一列：訓練用作監督的法線，與模型的法線並排。沒有任何基元直接算繪"
            "法線，所以右側是從算繪深度求出的法線——法線項比較的也正是它。"),
    KO("학습이 교사로 쓰는 법선과 모델의 법선을 나란히 놓은 줄을 켭니다. 법선을 "
       "직접 그리는 프리미티브는 없으므로 오른쪽은 렌더링한 깊이에서 구한 법선이고, "
       "법선 항이 비교하는 것도 그것입니다."),
    DE("Eine Zeile für die Normalen, gegen die trainiert wird, neben denen des "
       "Modells. Kein Primitiv rendert Normalen, die rechte Fläche zeigt also "
       "die aus der gerenderten Tiefe abgeleiteten -- genau die, die der "
       "Normalenterm vergleicht."),
    FR("Une rangée pour les normales qui servent de référence, à côté de celles "
       "du modèle. Aucune primitive ne rend de normales : le panneau de droite "
       "montre celles dérivées de la profondeur rendue, celles-là mêmes que "
       "compare le terme de normales."),
    ES("Una fila para las normales con las que se supervisa, junto a las del "
       "modelo. Ninguna primitiva renderiza normales, así que el panel derecho "
       "muestra las derivadas de la profundidad renderizada, que son las que "
       "compara el término de normales."),
    PT("Uma linha para as normais que servem de referência, ao lado das do "
       "modelo. Nenhuma primitiva renderiza normais, por isso o painel da "
       "direita mostra as derivadas da profundidade renderizada, que são as que "
       "o termo de normais compara."),
    IT("Una riga per le normali usate come riferimento, accanto a quelle del "
       "modello. Nessuna primitiva rende normali, quindi il riquadro di destra "
       "mostra quelle ricavate dalla profondità resa, le stesse che il termine "
       "di normali confronta."),
    NL("Een rij voor de normalen waartegen de run traint, naast die van het "
       "model. Geen enkele primitief rendert normalen, dus het rechterpaneel "
       "toont die uit de gerenderde diepte afgeleid -- precies wat de "
       "normaalterm vergelijkt."),
    RU("Ряд для нормалей, по которым идёт обучение, рядом с нормалями модели. "
       "Ни один примитив не рисует нормали, поэтому справа — выведенные из "
       "отрисованной глубины, те самые, что сравнивает член нормалей."),
    TR("Eğitimin denetim için kullandığı normaller ile modelin normallerini yan "
       "yana koyan bir satır. Hiçbir ilkel normal görüntülemez; sağdaki bölme, "
       "görüntülenen derinlikten türetilenleri gösterir -- normal teriminin "
       "karşılaştırdığı da bunlardır."));

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

}  // namespace gui
}  // namespace msg
}  // namespace i18n
}  // namespace spirula

#include "i18n/EndCatalog.h"
