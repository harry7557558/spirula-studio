#pragma once

// What `spirula sfm --help` and `spirula sfm <command> --help` say around the
// flag table -- the command summaries, the descriptions, the labels of each
// block, and the hand-parsed options each command owns.
//
// The per-flag sentences live next door in i18n/catalog/SfmFields.h, one per
// row of SFM_CONFIG_FIELDS. Here is everything else.
//
// What stays as it is, in every language:
//   - the ARGUMENT SYNTAX (`<MATCHES.BIN> <FEATURE_DIR> -o SPARSE_DIR`) and the
//     EXAMPLES. They are what the reader types, character for character.
//   - flag names, file names, format names, and the D-numbers, which cite the
//     decision records in src/sfm/README.md.
//
// Descriptions are written as ONE STRING PER PARAGRAPH, separated by a blank
// line, and wrapped at print time by i18n::wrap -- which measures in terminal
// columns and can break Chinese and Japanese, neither of which a hand-wrapped
// English paragraph could have done.

#include "i18n/BeginCatalog.h"

namespace spirula {
namespace i18n {
namespace msg {
namespace sfmhelp {

// ===========================================================================
// The labels a help page is built from
// ===========================================================================

SS_MSG(label_usage,
    EN("Usage:"), JA("使い方:"), ZH_HANS("用法："), ZH_HANT("用法："),
    KO("사용법:"), DE("Aufruf:"), FR("Utilisation :"), ES("Uso:"), PT("Uso:"),
    IT("Uso:"), NL("Gebruik:"), RU("Использование:"), TR("Kullanım:"));

SS_MSG(label_description,
    EN("Description:"), JA("説明:"), ZH_HANS("说明："), ZH_HANT("說明："),
    KO("설명:"), DE("Beschreibung:"), FR("Description :"), ES("Descripción:"),
    PT("Descrição:"), IT("Descrizione:"), NL("Beschrijving:"), RU("Описание:"),
    TR("Açıklama:"));

SS_MSG(label_options,
    EN("Options:"), JA("オプション:"), ZH_HANS("选项："), ZH_HANT("選項："),
    KO("옵션:"), DE("Optionen:"), FR("Options :"), ES("Opciones:"),
    PT("Opções:"), IT("Opzioni:"), NL("Opties:"), RU("Параметры:"),
    TR("Seçenekler:"));

SS_MSG(label_examples,
    EN("Examples:"), JA("例:"), ZH_HANS("示例："), ZH_HANT("範例："),
    KO("예:"), DE("Beispiele:"), FR("Exemples :"), ES("Ejemplos:"),
    PT("Exemplos:"), IT("Esempi:"), NL("Voorbeelden:"), RU("Примеры:"),
    TR("Örnekler:"));

SS_MSG(label_commands,
    EN("Commands:"), JA("コマンド:"), ZH_HANS("命令："), ZH_HANT("命令："),
    KO("명령:"), DE("Befehle:"), FR("Commandes :"), ES("Órdenes:"),
    PT("Comandos:"), IT("Comandi:"), NL("Opdrachten:"), RU("Команды:"),
    TR("Komutlar:"));

SS_MSG(label_environment,
    EN("Environment:"), JA("環境変数:"), ZH_HANS("环境变量："),
    ZH_HANT("環境變數："), KO("환경 변수:"), DE("Umgebung:"),
    FR("Environnement :"), ES("Entorno:"), PT("Ambiente:"), IT("Ambiente:"),
    NL("Omgeving:"), RU("Переменные окружения:"), TR("Ortam:"));

SS_MSG(label_exit_status,
    EN("Exit status:"), JA("終了ステータス:"), ZH_HANS("退出状态："),
    ZH_HANT("結束狀態："), KO("종료 상태:"), DE("Rückgabewert:"),
    FR("Code de sortie :"), ES("Estado de salida:"), PT("Estado de saída:"),
    IT("Stato di uscita:"), NL("Afsluitstatus:"), RU("Код возврата:"),
    TR("Çıkış durumu:"));

SS_MSG(word_required,
    EN("required"), JA("必須"), ZH_HANS("必填"), ZH_HANT("必填"), KO("필수"),
    DE("erforderlich"), FR("obligatoire"), ES("obligatorio"),
    PT("obrigatório"), IT("obbligatorio"), NL("verplicht"),
    RU("обязательно"), TR("zorunlu"));

SS_MSG(opt_help,
    EN("Show this help and exit."),
    JA("このヘルプを表示して終了します。"),
    ZH_HANS("显示这份帮助并退出。"),
    ZH_HANT("顯示這份說明並結束。"),
    KO("이 도움말을 보이고 끝냅니다."),
    DE("Diese Hilfe anzeigen und beenden."),
    FR("Afficher cette aide et quitter."),
    ES("Mostrar esta ayuda y salir."),
    PT("Mostrar esta ajuda e sair."),
    IT("Mostrare questo aiuto e uscire."),
    NL("Deze hulp tonen en stoppen."),
    RU("Показать эту справку и выйти."),
    TR("Bu yardımı göster ve çık."));

SS_MSG(opt_version,
    EN("Show the version and exit."),
    JA("バージョンを表示して終了します。"),
    ZH_HANS("显示版本并退出。"),
    ZH_HANT("顯示版本並結束。"),
    KO("버전을 보이고 끝냅니다."),
    DE("Die Version anzeigen und beenden."),
    FR("Afficher la version et quitter."),
    ES("Mostrar la versión y salir."),
    PT("Mostrar a versão e sair."),
    IT("Mostrare la versione e uscire."),
    NL("De versie tonen en stoppen."),
    RU("Показать версию и выйти."),
    TR("Sürümü göster ve çık."));

SS_MSG(tagline,
    EN("Structure from Motion for Spirula Studio"),
    JA("Spirula Studio のための Structure from Motion"),
    ZH_HANS("面向 Spirula Studio 的运动恢复结构"),
    ZH_HANT("面向 Spirula Studio 的運動恢復結構"),
    KO("Spirula Studio 를 위한 Structure from Motion"),
    DE("Structure from Motion für Spirula Studio"),
    FR("Structure from Motion pour Spirula Studio"),
    ES("Structure from Motion para Spirula Studio"),
    PT("Structure from Motion para o Spirula Studio"),
    IT("Structure from Motion per Spirula Studio"),
    NL("Structure from Motion voor Spirula Studio"),
    RU("Structure from Motion для Spirula Studio"),
    TR("Spirula Studio için Structure from Motion"));

SS_MSG(top_note,
    EN("Every stage reads and writes COLMAP's formats, so any one of them can be "
       "replaced by COLMAP's equivalent to bisect a failure. `auto` runs all of "
       "them."),
    JA("どの段階も COLMAP の形式で読み書きするため、失敗の切り分けにはどれか 1 つを "
       "COLMAP の対応するツールに置き換えられます。`auto` はそのすべてを走らせます。"),
    ZH_HANS("每个阶段都以 COLMAP 的格式读写，因此排查失败时可以把其中任何一个换成 "
            "COLMAP 的对应工具。`auto` 会把它们全部跑一遍。"),
    ZH_HANT("每個階段都以 COLMAP 的格式讀寫，因此排查失敗時可以把其中任何一個換成 "
            "COLMAP 的對應工具。`auto` 會把它們全部跑一遍。"),
    KO("모든 단계가 COLMAP 형식으로 읽고 쓰므로, 실패를 좁혀 갈 때 어느 하나를 "
       "COLMAP 의 대응 도구로 바꿔 넣을 수 있습니다. `auto` 는 그 전부를 돌립니다."),
    DE("Jede Stufe liest und schreibt COLMAPs Formate, also lässt sich jede "
       "einzelne durch COLMAPs Gegenstück ersetzen, um einen Fehler "
       "einzukreisen. `auto` führt sie alle aus."),
    FR("Chaque étape lit et écrit les formats de COLMAP : n'importe laquelle "
       "peut donc être remplacée par son équivalent COLMAP pour cerner une "
       "panne. `auto` les enchaîne toutes."),
    ES("Cada etapa lee y escribe los formatos de COLMAP, así que cualquiera de "
       "ellas puede sustituirse por su equivalente en COLMAP para acotar un "
       "fallo. `auto` las ejecuta todas."),
    PT("Cada etapa lê e escreve os formatos do COLMAP, então qualquer uma delas "
       "pode ser trocada pelo equivalente do COLMAP para isolar uma falha. "
       "`auto` executa todas elas."),
    IT("Ogni fase legge e scrive i formati di COLMAP, quindi ciascuna può essere "
       "sostituita dall'equivalente di COLMAP per circoscrivere un guasto. "
       "`auto` le esegue tutte."),
    NL("Elke fase leest en schrijft COLMAP's formaten, dus elk ervan kan door "
       "COLMAP's tegenhanger vervangen worden om een storing in te kaderen. "
       "`auto` draait ze allemaal."),
    RU("Каждая стадия читает и пишет форматы COLMAP, поэтому любую из них можно "
       "заменить соответствующим инструментом COLMAP, чтобы локализовать сбой. "
       "`auto` выполняет их все."),
    TR("Her aşama COLMAP'in biçimlerini okur ve yazar; bu yüzden bir arızayı "
       "daraltmak için herhangi biri COLMAP'teki karşılığıyla değiştirilebilir. "
       "`auto` hepsini çalıştırır."));

SS_MSG(env_map_prof,
    EN("print a per-stage breakdown of the mapper's time"),
    JA("マッパーの所要時間を段階ごとに分けて表示します"),
    ZH_HANS("按阶段拆分打印建图器的耗时"),
    ZH_HANT("按階段拆分列印建圖器的耗時"),
    KO("매퍼가 쓴 시간을 단계별로 나누어 출력합니다"),
    DE("die Zeit des Kartierers nach Stufen aufgeschlüsselt ausgeben"),
    FR("afficher le temps du cartographe détaillé par étape"),
    ES("mostrar el tiempo del cartógrafo desglosado por etapa"),
    PT("mostrar o tempo do cartógrafo detalhado por etapa"),
    IT("stampare il tempo del cartografo suddiviso per fase"),
    NL("de tijd van de kaartmaker per fase uitgesplitst tonen"),
    RU("печатать время построителя с разбивкой по стадиям"),
    TR("haritalayıcının süresini aşama aşama dökerek yazdır"));

// ===========================================================================
// What each command is, in one line
// ===========================================================================

SS_MSG(sum_auto,
    EN("reconstruct a sparse model from a directory of images, in one command"),
    JA("画像のディレクトリから疎なモデルを 1 つのコマンドで再構成します"),
    ZH_HANS("用一条命令从图像目录重建稀疏模型"),
    ZH_HANT("用一條命令從影像目錄重建稀疏模型"),
    KO("이미지 디렉터리에서 성긴 모델을 한 명령으로 재구성합니다"),
    DE("aus einem Bildverzeichnis in einem Befehl ein dünnes Modell "
       "rekonstruieren"),
    FR("reconstruire un modèle épars depuis un dossier d'images, en une commande"),
    ES("reconstruir un modelo disperso desde una carpeta de imágenes, en una "
       "sola orden"),
    PT("reconstruir um modelo esparso a partir de uma pasta de imagens, num só "
       "comando"),
    IT("ricostruire un modello sparso da una cartella di immagini, in un solo "
       "comando"),
    NL("een ijl model reconstrueren uit een beeldmap, in één opdracht"),
    RU("построить разреженную модель из папки изображений одной командой"),
    TR("bir görüntü dizininden tek komutla seyrek model oluştur"));

SS_MSG(sum_extract,
    EN("detect features in an image or a directory of images"),
    JA("画像 1 枚、または画像のディレクトリから特徴点を検出します"),
    ZH_HANS("在一张图像或一个图像目录中检测特征点"),
    ZH_HANT("在一張影像或一個影像目錄中偵測特徵點"),
    KO("이미지 하나 또는 이미지 디렉터리에서 특징점을 검출합니다"),
    DE("Merkmale in einem Bild oder einem Bildverzeichnis finden"),
    FR("détecter les points d'intérêt dans une image ou un dossier d'images"),
    ES("detectar rasgos en una imagen o en una carpeta de imágenes"),
    PT("detectar traços numa imagem ou numa pasta de imagens"),
    IT("rilevare i punti in un'immagine o in una cartella di immagini"),
    NL("kenmerken opsporen in een beeld of een beeldmap"),
    RU("найти признаки в изображении или в папке изображений"),
    TR("bir görüntüde ya da görüntü dizininde öznitelik bul"));

SS_MSG(sum_match,
    EN("match and geometrically verify a directory of feature files"),
    JA("特徴点ファイルのディレクトリをマッチングし、幾何的に検証します"),
    ZH_HANS("对一个特征点文件目录做匹配与几何验证"),
    ZH_HANT("對一個特徵點檔案目錄做匹配與幾何驗證"),
    KO("특징점 파일 디렉터리를 매칭하고 기하적으로 검증합니다"),
    DE("ein Verzeichnis von Merkmalsdateien zuordnen und geometrisch prüfen"),
    FR("apparier et vérifier géométriquement un dossier de fichiers de points"),
    ES("emparejar y verificar geométricamente una carpeta de archivos de rasgos"),
    PT("emparelhar e verificar geometricamente uma pasta de arquivos de traços"),
    IT("abbinare e verificare geometricamente una cartella di file di punti"),
    NL("een map met kenmerkbestanden matchen en meetkundig verifiëren"),
    RU("сопоставить и геометрически проверить каталог файлов признаков"),
    TR("bir öznitelik dosyası dizinini eşleştir ve geometrik olarak doğrula"));

SS_MSG(sum_map,
    EN("incremental reconstruction: matches to a COLMAP sparse model"),
    JA("逐次再構成: 対応から COLMAP の疎なモデルへ"),
    ZH_HANS("增量重建：从匹配到 COLMAP 稀疏模型"),
    ZH_HANT("增量重建：從匹配到 COLMAP 稀疏模型"),
    KO("증분 재구성: 대응에서 COLMAP 성긴 모델로"),
    DE("inkrementelle Rekonstruktion: von Zuordnungen zu einem dünnen "
       "COLMAP-Modell"),
    FR("reconstruction incrémentale : des appariements vers un modèle épars "
       "COLMAP"),
    ES("reconstrucción incremental: de los emparejamientos a un modelo disperso "
       "de COLMAP"),
    PT("reconstrução incremental: das correspondências a um modelo esparso do "
       "COLMAP"),
    IT("ricostruzione incrementale: dagli abbinamenti a un modello sparso COLMAP"),
    NL("incrementele reconstructie: van matches naar een ijl COLMAP-model"),
    RU("инкрементальная реконструкция: от соответствий к разреженной модели "
       "COLMAP"),
    TR("artımlı yeniden oluşturma: eşleşmelerden COLMAP seyrek modeline"));

SS_MSG(sum_merge,
    EN("fuse the models of a fragmented capture into fewer"),
    JA("断片化した撮影のモデルをより少ない数に融合します"),
    ZH_HANS("把零散拍摄产生的多个模型融合成更少的模型"),
    ZH_HANT("把零散拍攝產生的多個模型融合成更少的模型"),
    KO("조각난 촬영의 모델들을 더 적은 수로 합칩니다"),
    DE("die Modelle einer zerfallenen Aufnahme zu weniger verschmelzen"),
    FR("fondre les modèles d'une prise de vue fragmentée en un plus petit nombre"),
    ES("fundir los modelos de una captura fragmentada en menos modelos"),
    PT("fundir os modelos de uma captura fragmentada em menos modelos"),
    IT("fondere i modelli di una ripresa frammentata in un numero minore"),
    NL("de modellen van een versnipperde opname tot minder samensmelten"),
    RU("свести модели раздробленной съёмки к меньшему числу"),
    TR("parçalanmış bir çekimin modellerini daha aza kaynaştır"));

SS_MSG(sum_ba,
    EN("bundle-adjust a sparse model"),
    JA("疎なモデルをバンドル調整します"),
    ZH_HANS("对稀疏模型做光束法平差"),
    ZH_HANT("對稀疏模型做光束法平差"),
    KO("성긴 모델을 번들 조정합니다"),
    DE("ein dünnes Modell bündelausgleichen"),
    FR("ajuster les faisceaux d'un modèle épars"),
    ES("ajustar los haces de un modelo disperso"),
    PT("ajustar os feixes de um modelo esparso"),
    IT("eseguire il bundle adjustment di un modello sparso"),
    NL("een ijl model bundelaanpassen"),
    RU("уравнять разреженную модель"),
    TR("seyrek bir modeli demet dengelemesinden geçir"));

// ===========================================================================
// What each command does, at length
// ===========================================================================

SS_MSG(desc_auto_1,
    EN("Runs extract → match → map → merge with COLMAP’s automatic_reconstructor presets.\n- Writes WORKSPACE/{features,matches.bin,sparse/0}.\n- Disconnected components go to sparse/1, sparse/2, etc., largest first."),
    JA("extract -> match -> map -> merge を COLMAP の automatic_reconstructor プリセットで走らせ、WORKSPACE/{features,matches.bin,sparse/0} を書き出します。\n1 つの連結した視点グラフになっていない撮影では sparse/1、sparse/2、... も書き出します（残りの成分を大きい順に）。"),
    ZH_HANS("按 COLMAP 的 automatic_reconstructor 预设依次运行 extract -> match -> map -> merge，并写出 WORKSPACE/{features,matches.bin,sparse/0}。\n若拍摄不是一个连通的视图图，还会写出 sparse/1、sparse/2……即其余的连通块，从大到小。"),
    ZH_HANT("按 COLMAP 的 automatic_reconstructor 預設依次執行 extract -> match -> map -> merge，並寫出 WORKSPACE/{features,matches.bin,sparse/0}。\n若拍攝不是一個連通的視圖圖，還會寫出 sparse/1、sparse/2……即其餘的連通塊，從大到小。"),
    KO("extract -> match -> map -> merge 를 COLMAP 의 automatic_reconstructor 프리셋으로 돌리고 WORKSPACE/{features,matches.bin,sparse/0} 을 씁니다.\n하나로 이어진 뷰 그래프가 아닌 촬영이면 sparse/1, sparse/2, ... 도 씁니다 -- 남은 성분을 큰 것부터."),
    DE("Führt extract -> match -> map -> merge mit den Voreinstellungen von COLMAPs automatic_reconstructor aus und schreibt WORKSPACE/{features,matches.bin,sparse/0}.\nEine Aufnahme, die kein zusammenhängender Sichtgraph ist, schreibt zusätzlich sparse/1, sparse/2, ... -- die übrigen Komponenten, die größte zuerst."),
    FR("Enchaîne extract -> match -> map -> merge avec les préréglages de l'automatic_reconstructor de COLMAP et écrit WORKSPACE/{features,matches.bin,sparse/0}.\nUne prise de vue qui n'est pas un seul graphe de vues connexe écrit aussi sparse/1, sparse/2, ... -- les composantes restantes, la plus grande d'abord."),
    ES("Ejecuta extract -> match -> map -> merge con los ajustes del automatic_reconstructor de COLMAP y escribe WORKSPACE/{features,matches.bin,sparse/0}.\nUna captura que no es un único grafo de vistas conexo escribe además sparse/1, sparse/2, ...: las componentes restantes, la mayor primero."),
    PT("Executa extract -> match -> map -> merge com as predefinições do automatic_reconstructor do COLMAP e escreve WORKSPACE/{features,matches.bin,sparse/0}.\nUma captura que não é um único grafo de vistas conexo escreve também sparse/1, sparse/2, ...: os componentes restantes, o maior primeiro."),
    IT("Esegue extract -> match -> map -> merge con le preimpostazioni dell'automatic_reconstructor di COLMAP e scrive WORKSPACE/{features,matches.bin,sparse/0}.\nUna ripresa che non è un unico grafo delle viste connesso scrive anche sparse/1, sparse/2, ...: le componenti restanti, la più grande per prima."),
    NL("Draait extract -> match -> map -> merge met de voorinstellingen van COLMAP's automatic_reconstructor en schrijft WORKSPACE/{features,matches.bin,sparse/0}.\nEen opname die geen samenhangende zichtgraaf is, schrijft ook sparse/1, sparse/2, ... -- de overige componenten, de grootste eerst."),
    RU("Выполняет extract -> match -> map -> merge с предустановками automatic_reconstructor из COLMAP и пишет WORKSPACE/{features,matches.bin,sparse/0}.\nСъёмка, не образующая один связный граф видов, даёт ещё sparse/1, sparse/2, ... -- остальные компоненты, начиная с наибольшей."),
    TR("extract -> match -> map -> merge adımlarını COLMAP'in automatic_reconstructor ön ayarlarıyla çalıştırır ve WORKSPACE/{features,matches.bin,sparse/0} yazar.\nTek bir bağlı görüş çizgesi olmayan bir çekim ayrıca sparse/1, sparse/2, ... yazar -- kalan bileşenler, en büyükten başlayarak."));

SS_MSG(desc_auto_2,
    EN("Input defaults to `images`. `auto DATASET` uses DATASET/images just like `auto DATASET/images`.\nA `masks` folder beside the image directory is picked up automatically."),
    JA("位置引数の既定は `images` です。 `images/` を含むデータセットディレクトリも受け付け、そのサブディレクトリを使うため、`auto DATASET` と `auto DATASET/images` は同じ動きになります。\n画像ディレクトリの隣にある `masks` は自動的に拾われます。"),
    ZH_HANS("位置参数默认为 `images`；也接受包含 `images/` 的数据集目录并改用该子目录，因此 `auto DATASET` 与 `auto DATASET/images` 行为相同。\n图像目录旁边的 `masks` 会被自动采用。"),
    ZH_HANT("位置參數預設為 `images`；也接受包含 `images/` 的資料集目錄並改用該子目錄，因此 `auto DATASET` 與 `auto DATASET/images` 行為相同。\n影像目錄旁邊的 `masks` 會被自動採用。"),
    KO("위치 인자의 기본값은 `images` 입니다. `images/` 를 담은 데이터셋 디렉터리도 받아 그 하위 디렉터리를 쓰므로 `auto DATASET` 과 `auto DATASET/images` 는 같게 동작합니다.\n이미지 디렉터리 옆의 `masks` 는 저절로 집어 옵니다."),
    DE("Das Positionsargument ist standardmäßig `images`; ein Datensatzverzeichnis mit `images/` wird ebenfalls angenommen und jenes Unterverzeichnis benutzt, sodass `auto DATASET` und `auto DATASET/images` gleich wirken.\nEin `masks` neben dem Bildverzeichnis wird von selbst aufgegriffen."),
    FR("Le positionnel vaut `images` par défaut ; un dossier de jeu de données contenant `images/` est accepté et ce sous-dossier employé, si bien que `auto DATASET` et `auto DATASET/images` se comportent pareil.\nUn `masks` à côté du dossier d'images est repris automatiquement."),
    ES("El posicional vale `images` por defecto; también se acepta una carpeta de conjunto de datos que contenga `images/` y se usa esa subcarpeta, así que `auto DATASET` y `auto DATASET/images` se comportan igual.\nUn `masks` junto a la carpeta de imágenes se recoge solo."),
    PT("O posicional vale `images` por padrão; também se aceita uma pasta de conjunto de dados que contenha `images/` e usa-se essa subpasta, de modo que `auto DATASET` e `auto DATASET/images` se comportam igual.\nUm `masks` ao lado da pasta de imagens é apanhado sozinho."),
    IT("Il posizionale vale `images` per impostazione predefinita; si accetta anche una cartella di dataset che contenga `images/` e si usa quella sottocartella, così `auto DATASET` e `auto DATASET/images` si comportano allo stesso modo.\nUn `masks` accanto alla cartella delle immagini viene raccolto da sé."),
    NL("Het positionele argument is standaard `images`; een datasetmap met `images/` erin wordt ook aanvaard en die submap gebruikt, zodat `auto DATASET` en `auto DATASET/images` hetzelfde doen.\nEen `masks` naast de beeldmap wordt vanzelf opgepakt."),
    RU("Позиционный аргумент по умолчанию -- `images`; принимается и каталог набора данных, содержащий `images/`, и берётся этот подкаталог, так что `auto DATASET` и `auto DATASET/images` ведут себя одинаково.\n`masks` рядом с каталогом изображений подхватывается сам."),
    TR("Konumsal argüman varsayılan olarak `images`'tır; `images/` içeren bir veri kümesi dizini de kabul edilir ve o alt dizin kullanılır, böylece `auto DATASET` ile `auto DATASET/images` aynı davranır.\nGörüntü dizininin yanındaki `masks` kendiliğinden alınır."));

SS_MSG(desc_auto_3,
    EN("Two knobs decide the rest: --quality and --data-type.\nAnything they set can be overridden by naming the flag explicitly, and the run reports what they moved."),
    JA("残りは --quality と --data-type の 2 つのつまみが決めます。\nこれらが設定したものはフラグを明示すれば上書きでき、実行時にはそれらが動かした内容が報告されます。"),
    ZH_HANS("其余由两个旋钮决定：--quality 与 --data-type。\n它们设定的任何值都可以通过显式写出对应标志来覆盖，运行时也会报告它们改动了什么。"),
    ZH_HANT("其餘由兩個旋鈕決定：--quality 與 --data-type。\n它們設定的任何值都可以透過明確寫出對應旗標來覆蓋，執行時也會回報它們改動了什麼。"),
    KO("나머지는 --quality 와 --data-type 두 손잡이가 정합니다.\n이들이 정한 값은 해당 플래그를 직접 쓰면 덮어쓸 수 있고, 실행 중에 무엇을 옮겼는지 알려 줍니다."),
    DE("--quality und --data-type setzen die Vorgaben. Jede Option kann explizit überschrieben werden; der Lauf meldet geänderte Vorgaben."),
    FR("--quality et --data-type fixent les réglages. Chaque option explicite les remplace ; le traitement signale les réglages modifiés."),
    ES("Dos mandos deciden el resto: --quality y --data-type. Todo lo que fijan puede sustituirse nombrando la opción de forma explícita, y la ejecución informa de lo que movieron."),
    PT("Dois botões decidem o resto: --quality e --data-type. Tudo o que eles fixam pode ser substituído nomeando a opção explicitamente, e a execução relata o que eles moveram."),
    IT("--quality e --data-type fissano le opzioni. Ogni flag esplicito le sostituisce; l’esecuzione segnala i valori modificati."),
    NL("--quality en --data-type bepalen de opties. Elke expliciete optie overschrijft ze; de run meldt gewijzigde instellingen."),
    RU("Остальное решают две ручки: --quality и --data-type.\nВсё, что они задают, можно перекрыть, назвав флаг явно, а запуск сообщает, что именно они сдвинули."),
    TR("Gerisini iki düğme belirler: --quality ve --data-type. Bunların belirlediği her şey, ilgili bayrağı açıkça yazarak değiştirilebilir ve çalışma neyi oynattıklarını bildirir."));

SS_MSG(desc_extract_1,
    EN("Run GPU SIFT on one image or a directory tree.\n- A directory shares one GPU context and processes largest images first, reusing buffers.\n- --decode-budget limits decoder memory. Peak memory stays bounded as images are added.\n- Feature files mirror the image tree."),
    JA("画像 1 枚、またはディレクトリを再帰的にたどって GPU 上で SIFT を実行します。ディレクトリの場合は GPU コンテキストを 1 つ使い回し、大きいものから処理するため、デバイス側のバッファは一度だけ確保されます。\n- デコードは --decode-budget に収まる大きさのプールで動き、ピークメモリは画像枚数とともに増えません。\n- 特徴点ファイルは画像のツリー構造をそのまま写します。"),
    ZH_HANS("在 GPU 上对单张图像执行 SIFT，或递归遍历一个目录。处理目录时复用同一个 GPU 上下文并从大到小处理，因此设备缓冲只分配一次；\n解码在一个按 --decode-budget 定大小的线程池上进行，峰值内存不随图像数量增长。特征点文件与图像目录结构一一对应。"),
    ZH_HANT("在 GPU 上對單張影像執行 SIFT，或遞迴走訪一個目錄。處理目錄時重用同一個 GPU 內容並從大到小處理，因此裝置緩衝只配置一次；\n- 解碼在一個按 --decode-budget 定大小的執行緒池上進行，尖峰記憶體不隨影像數量增長。\n- 特徵點檔案與影像目錄結構一一對應。"),
    KO("이미지 하나 또는 디렉터리를 재귀로 돌며 GPU 에서 SIFT 를 실행합니다. 디렉터리를 처리할 때는 GPU 컨텍스트 하나를 재사용하고 큰 것부터 처리하므로 장치 버퍼는 한 번만 잡습니다.\n- 디코딩은 --decode-budget 에 맞춘 풀에서 돌고, 최대 메모리는 이미지 수에 따라 늘지 않습니다.\n- 특징점 파일은 이미지 트리를 그대로 본뜹니다."),
    DE("GPU-SIFT über ein Bild oder, rekursiv, über ein Verzeichnis. Ein Verzeichnis nutzt einen GPU-Kontext wieder und arbeitet die größten zuerst ab, sodass Gerätepuffer nur einmal belegt werden;\ndas Dekodieren läuft auf einem Pool in der Größe von --decode-budget, und der Spitzenspeicher wächst nicht mit der Bildzahl. Die Merkmalsdateien spiegeln den Bildbaum."),
    FR("SIFT sur GPU pour une image ou, récursivement, pour un dossier. Un dossier réutilise un seul contexte GPU et traite les plus grandes d'abord, si bien que les tampons du périphérique ne sont alloués qu'une fois ; le décodage tourne sur une réserve taillée pour --decode-budget, et la mémoire de pointe ne croît pas avec le nombre d'images.\nLes fichiers de points reproduisent l'arborescence des images."),
    ES("SIFT en la GPU sobre una imagen o, de forma recursiva, sobre una carpeta. Una carpeta reutiliza un solo contexto de GPU y procesa las mayores primero, así que los búferes del dispositivo se reservan una vez;\n- la descodificación corre en una reserva ajustada a --decode-budget, y la memoria de pico no crece con el número de imágenes.\n- Los archivos de rasgos reflejan el árbol de imágenes."),
    PT("SIFT na GPU sobre uma imagem ou, recursivamente, sobre uma pasta. Uma pasta reaproveita um único contexto de GPU e processa as maiores primeiro, então os buffers do dispositivo são reservados uma vez;\n- a decodificação corre numa reserva ajustada a --decode-budget, e a memória de pico não cresce com o número de imagens.\n- Os arquivos de traços espelham a árvore de imagens."),
    IT("SIFT su GPU per un'immagine o, ricorsivamente, per una cartella. Una cartella riusa un solo contesto GPU e lavora prima le più grandi, così i buffer del dispositivo si allocano una volta sola;\n- la decodifica gira su un pool dimensionato su --decode-budget, e la memoria di picco non cresce col numero di immagini.\n- I file dei punti rispecchiano l'albero delle immagini."),
    NL("GPU-SIFT over één beeld of, recursief, over een map. Een map hergebruikt één GPU-context en verwerkt de grootste eerst, zodat apparaatbuffers eenmaal worden gereserveerd;\n- het decoderen draait op een pool ter grootte van --decode-budget, en het piekgeheugen groeit niet mee met het aantal beelden.\n- De kenmerkbestanden spiegelen de beeldboom."),
    RU("SIFT на GPU по одному изображению или, рекурсивно, по каталогу. Для каталога переиспользуется один контекст GPU, и обработка идёт от крупнейших, так что буферы устройства выделяются однажды;\n- декодирование работает в пуле под --decode-budget, а пиковая память не растёт с числом изображений.\n- Файлы признаков повторяют дерево изображений."),
    TR("Bir görüntü üzerinde ya da özyinelemeli olarak bir dizin üzerinde GPU SIFT. Bir dizin tek bir GPU bağlamını yeniden kullanır ve en büyükten başlayarak işler, böylece aygıt tamponları bir kez ayrılır;\n- çözme --decode-budget'a göre boyutlanmış bir havuzda çalışır ve tepe bellek görüntü sayısıyla büyümez.\n- Öznitelik dosyaları görüntü ağacını yansıtır."));

SS_MSG(desc_extract_2,
    EN("Keypoints use source-image coordinates, even with --max-image-size.\nEXIF camera and focal data let match and map build intrinsics without rereading images (D46)."),
    JA("--max-image-size を使っても、特徴点は元画像の座標です。\nmatch と map は記録した EXIF のカメラと焦点距離から内部パラメータを作ります。画像の再読込は不要です（D46）。"),
    ZH_HANS("即使使用 --max-image-size，特征点仍用原图坐标。\nmatch 与 map 用保存的 EXIF 相机和焦距数据建立内参，无需重读图像（D46）。"),
    ZH_HANT("即使使用 --max-image-size，特徵點仍用原圖座標。\nmatch 與 map 用儲存的 EXIF 相機和焦距資料建立內參，無需重讀影像（D46）。"),
    KO("--max-image-size를 써도 특징점은 원본 이미지 좌표입니다.\nmatch와 map은 저장된 EXIF 카메라와 초점 거리로 내부 파라미터를 구합니다. 이미지를 다시 읽지 않습니다(D46)."),
    DE("Merkmale bleiben mit --max-image-size in Originalbildkoordinaten.\nEXIF-Kamera und Brennweite liefern match und map die Intrinsik ohne erneutes Bildlesen (D46)."),
    FR("Points en coordonnées d’origine, même avec --max-image-size.\nLes données EXIF de focale et de caméra donnent les intrinsèques à match et map sans relire les images (D46)."),
    ES("Puntos en coordenadas originales, incluso con --max-image-size.\nCámara y focal EXIF dan los intrínsecos a match y map sin releer imágenes (D46)."),
    PT("Pontos nas coordenadas originais, mesmo com --max-image-size.\nCâmara e focal EXIF dão os intrínsecos a match e map sem reler imagens (D46)."),
    IT("Punti in coordinate originali, anche con --max-image-size.\nCamera e focale EXIF danno a match e map gli intrinseci senza rileggere le immagini (D46)."),
    NL("Kenmerken in bronbeeldcoördinaten, ook met --max-image-size.\nEXIF-camera en brandpunt geven match en map de intrinsieken zonder beelden te herlezen (D46)."),
    RU("Точки в координатах исходного снимка даже с --max-image-size.\nКамера и фокус EXIF дают match и map внутренние параметры без перечитывания снимков (D46)."),
    TR("--max-image-size ile de noktalar kaynak görüntü koordinatındadır.\nEXIF kamera ve odak verileri, match ve map için görüntüleri tekrar okumadan iç parametreleri verir (D46)."));

SS_MSG(desc_match_1,
    EN("GPU brute-force matching, then F/H RANSAC verification on a worker pool. --threads sets verification workers; results are independent of the count."),
    JA("GPU全探索の後、ワーカープールでF/H RANSAC検証。--threads が検証ワーカー数を設定します。結果は数に依存しません。"),
    ZH_HANS("GPU 暴力匹配后，由工作线程池进行 F/H RANSAC 验证。--threads 设置验证线程数，不影响结果。"),
    ZH_HANT("GPU 暴力匹配後，由工作執行緒池進行 F/H RANSAC 驗證。--threads 設定驗證執行緒數，不影響結果。"),
    KO("GPU 전수 매칭 후 작업 풀에서 F/H RANSAC 검증을 합니다. --threads는 검증 작업자 수이며 결과는 수에 무관합니다."),
    DE("GPU-Brute-Force-Abgleich, dann F/H-RANSAC-Prüfung im Workerpool. --threads setzt Prüfworker; Ergebnisse sind unabhängig von ihrer Zahl."),
    FR("Appariement exhaustif GPU puis vérification F/H RANSAC sur un pool. --threads fixe les workers ; le nombre ne change pas les résultats."),
    ES("Emparejamiento exhaustivo GPU y verificación F/H RANSAC en un grupo. --threads fija los hilos; el número no cambia los resultados."),
    PT("Correspondência exaustiva GPU e verificação F/H RANSAC em um pool. --threads define trabalhadores; o número não muda os resultados."),
    IT("Abbinamento esaustivo GPU, poi verifica F/H RANSAC su un pool. --threads fissa i worker; il numero non cambia i risultati."),
    NL("GPU-bruteforcekoppeling, dan F/H-RANSAC-controle in een pool. --threads stelt controleworkers in; hun aantal verandert de resultaten niet."),
    RU("Полный перебор на GPU, затем проверка F/H RANSAC в пуле. --threads задаёт число проверяющих потоков; оно не влияет на результат."),
    TR("GPU tam tarama eşleştirmesi, sonra havuzda F/H RANSAC doğrulaması. --threads doğrulama işçisi sayısıdır; sonuçlar sayıdan bağımsızdır."));

SS_MSG(desc_match_2,
    EN("Fisheye --camera-model verifies unit bearings at any field of view.\n- Estimates focal length from sampled pairs per camera group unless --focal or EXIF supplies it.\n- The match database carries camera groups and focals into map."),
    JA("魚眼の --camera-model を指定すると、検証は単位方位ベクトルに切り替わります。そこではエピポーラ拘束がどんな画角でも成り立ちます。--focal や EXIF が焦点距離を与えない場合は、ペアの標本上でカメラグループごとに探索します。\nこの段階が定めたカメラのグループ分けと焦点距離は対応データベースに入るので、`map` は導き直さずに引き継ぎます。"),
    ZH_HANS("指定鱼眼的 --camera-model 后，验证会改用单位方向向量，此时对极约束在任意视场角下都成立；若 --focal 或 EXIF 没有给出焦距，则在一部分像对上按相机组搜索。\n本阶段定下的相机分组与焦距会写入匹配数据库，因此 `map` 直接沿用而不再重新推导。"),
    ZH_HANT("指定魚眼的 --camera-model 後，驗證會改用單位方向向量，此時對極約束在任意視場角下都成立；若 --focal 或 EXIF 沒有給出焦距，則在一部分影像對上按相機群組搜尋。\n本階段定下的相機分組與焦距會寫入匹配資料庫，因此 `map` 直接沿用而不再重新推導。"),
    KO("어안 --camera-model 을 주면 검증이 단위 방향 벡터로 바뀌며, 그곳에서는 에피폴라 제약이 어떤 화각에서도 성립합니다. --focal 이나 EXIF 가 초점 거리를 주지 않으면 쌍의 표본에서 카메라 그룹별로 탐색합니다.\n이 단계가 정한 카메라 묶음과 초점 거리는 대응 데이터베이스에 실려 가므로, `map` 은 다시 끌어내지 않고 물려받습니다."),
    DE("Ein Fischaugen---camera-model schaltet die Prüfung auf Einheitsrichtungen um, wo die Epipolarbedingung bei jedem Bildwinkel gilt; die Brennweite wird je Kameragruppe an einer Stichprobe von Paaren gesucht, sofern nicht --focal oder EXIF eine liefert.\nDie Kameragruppierung und die Brennweiten, auf die sich diese Stufe festlegt, reisen in der Zuordnungsdatenbank mit, sodass `map` sie erbt, statt sie neu herzuleiten."),
    FR("Un --camera-model fisheye fait passer la vérification à des directions unitaires, où la contrainte épipolaire tient à n'importe quel champ ; la focale est cherchée par groupe de caméras sur un échantillon de paires, sauf si --focal ou l'EXIF en fournit une.\nLe regroupement des caméras et les focales arrêtés à cette étape voyagent dans la base d'appariements, si bien que `map` en hérite au lieu de les redériver."),
    ES("Un --camera-model de ojo de pez pasa la verificación a direcciones unitarias, donde la restricción epipolar se cumple con cualquier campo de visión;\n- la focal se busca por grupo de cámaras sobre una muestra de pares, salvo que --focal o el EXIF den una.\n- La agrupación de cámaras y las focales que fija esta etapa viajan en la base de emparejamientos, así que `map` las hereda en vez de rededucirlas."),
    PT("Um --camera-model olho de peixe passa a verificação a direções unitárias, onde a restrição epipolar vale em qualquer campo de visão;\n- a focal é procurada por grupo de câmeras numa amostra de pares, salvo se --focal ou o EXIF derem uma.\n- O agrupamento de câmeras e as focais que esta etapa fixa viajam na base de correspondências, então `map` herda-os em vez de os deduzir de novo."),
    IT("Un --camera-model fisheye porta la verifica su direzioni unitarie, dove il vincolo epipolare vale a qualunque campo visivo; la focale si cerca per gruppo di camere su un campione di coppie, a meno che --focal o l'EXIF non ne diano una.\nIl raggruppamento delle camere e le focali fissate da questa fase viaggiano nel database degli abbinamenti, così `map` le eredita invece di riderivarle."),
    NL("Een fisheye---camera-model zet de verificatie om naar eenheidsrichtingen, waar de epipolaire voorwaarde bij elk beeldveld geldt; de brandpuntsafstand wordt per cameragroep gezocht op een steekproef van paren, tenzij --focal of de EXIF er een geeft.\nDe cameragroepering en de brandpuntsafstanden die deze fase vastlegt reizen mee in de matchdatabase, zodat `map` ze erft in plaats van ze opnieuw af te leiden."),
    RU("Рыбий глаз в --camera-model переводит проверку на единичные направления, где эпиполярное условие выполняется при любом поле зрения;\n- фокусное расстояние ищется по группам камер на выборке пар, если его не дают --focal или EXIF.\n- Группировка камер и фокусные расстояния, на которых остановилась эта стадия, едут в базе соответствий, так что `map` наследует их, а не выводит заново."),
    TR("Balıkgözü bir --camera-model, doğrulamayı birim yön vektörlerine geçirir;\n- orada epipolar kısıt her görüş açısında geçerlidir. --focal ya da EXIF bir odak vermiyorsa, odak bir çift örneklemi üzerinde kamera grubu başına aranır.\n- Bu aşamanın karar kıldığı kamera gruplaması ve odaklar eşleşme veritabanıyla birlikte gider, böylece `map` onları yeniden türetmek yerine devralır."));

SS_MSG(desc_map_1,
    EN("Seed, register, triangulate, bundle-adjust and filter, then run merge and finishing passes.\n- Disconnected components go to <out>/0, <out>/1, etc., largest first.\n- Merges models with shared images and registers remaining views where possible."),
    JA("シード選び、登録、三角測量、バンドル調整、フィルタリング -- そのあとに統合の各段と仕上げのパスが続きます。\n- 1 つの連結した視点グラフになっていない撮影は複数のモデルとして再構成され、<out>/0、<out>/1、... に大きい順で書き出されます（COLMAP の配置）。\n- それらは画像を共有するところで統合され、さらに登録できるものを取り込んで成長します。"),
    ZH_HANS("选种子、注册、三角化、平差、过滤 —— 然后是各层合并与收尾阶段。若拍摄不是一个连通的视图图，就会重建成多个模型，按从大到小写入 <out>/0、<out>/1……（COLMAP 的布局）；\n随后在共享图像处把它们合并，并把还能注册的图像继续并入。"),
    ZH_HANT("選種子、註冊、三角化、平差、過濾 —— 然後是各層合併與收尾階段。若拍攝不是一個連通的視圖圖，就會重建成多個模型，按從大到小寫入 <out>/0、<out>/1……（COLMAP 的配置）；\n隨後在共享影像處把它們合併，並把還能註冊的影像繼續併入。"),
    KO("씨앗 고르기, 등록, 삼각측량, 번들 조정, 걸러내기 -- 그다음 병합 층들과 마무리 단계가 이어집니다. 하나로 이어진 뷰 그래프가 아닌 촬영은 여러 모델로 재구성되어 <out>/0, <out>/1, ... 에 큰 것부터 쓰입니다(COLMAP 의 배치).\n이후 이미지를 공유하는 곳에서 병합되고, 더 등록할 수 있는 것을 받아들이며 자랍니다."),
    DE("Startpaar, Registrieren, Triangulieren, Ausgleichen, Filtern -- dann die Zusammenführungsebenen und die Schlussdurchgänge.\n- Eine Aufnahme, die kein zusammenhängender Sichtgraph ist, wird zu mehreren Modellen, geschrieben nach <out>/0, <out>/1, ... das größte zuerst (COLMAPs Anordnung);\n- sie werden dort zusammengeführt, wo sie Bilder teilen, und um alles erweitert, was sie sonst noch registrieren können."),
    FR("Amorcer, enregistrer, trianguler, ajuster, filtrer -- puis les niveaux de fusion et les passes de finition.\n- Une prise de vue qui n'est pas un seul graphe de vues connexe se reconstruit en plusieurs modèles, écrits dans <out>/0, <out>/1, ... du plus grand au plus petit (la disposition de COLMAP) ;\n- ils sont ensuite fusionnés là où ils partagent des images, et étendus à tout ce qu'ils peuvent encore enregistrer."),
    ES("Sembrar, registrar, triangular, ajustar, filtrar; luego los niveles de fusión y las pasadas de acabado. Una captura que no es un único grafo de vistas conexo se reconstruye como varios modelos, escritos en <out>/0, <out>/1, ... del mayor al menor (la disposición de COLMAP);\ndespués se fusionan donde comparten imágenes y crecen con todo lo que aún puedan registrar."),
    PT("Semear, registrar, triangular, ajustar, filtrar -- depois os níveis de fusão e as passagens de acabamento. Uma captura que não é um único grafo de vistas conexo reconstrói-se como vários modelos, escritos em <out>/0, <out>/1, ... do maior para o menor (a disposição do COLMAP);\nem seguida são fundidos onde partilham imagens e crescem com tudo o que ainda conseguirem registrar."),
    IT("Innescare, registrare, triangolare, aggiustare, filtrare -- poi i livelli di fusione e i passaggi di rifinitura.\n- Una ripresa che non è un unico grafo delle viste connesso si ricostruisce come più modelli, scritti in <out>/0, <out>/1, ... dal più grande (la disposizione di COLMAP);\n- vengono poi fusi dove condividono immagini e accresciuti con tutto ciò che riescono ancora a registrare."),
    NL("Zaaien, registreren, trianguleren, aanpassen, filteren -- daarna de samenvoegniveaus en de afwerkgangen.\n- Een opname die geen samenhangende zichtgraaf is, wordt gereconstrueerd als meerdere modellen, geschreven naar <out>/0, <out>/1, ... grootste eerst (COLMAP's indeling);\n- daarna worden ze samengevoegd waar ze beelden delen en uitgebreid met al wat ze nog kunnen registreren."),
    RU("Начальная пара, регистрация, триангуляция, уравнивание, отсев -- затем уровни слияния и завершающие проходы. Съёмка, не образующая один связный граф видов, восстанавливается как несколько моделей, записываемых в <out>/0, <out>/1, ... начиная с наибольшей (раскладка COLMAP);\nпотом их сливают там, где у них общие изображения, и наращивают всем, что они ещё могут зарегистрировать."),
    TR("Tohumla, kaydet, üçgenle, dengele, ele -- ardından birleştirme düzeyleri ve bitirme geçişleri.\n- Tek bir bağlı görüş çizgesi olmayan bir çekim birden çok model olarak oluşturulur ve <out>/0, <out>/1, ... içine en büyükten başlayarak yazılır (COLMAP'in düzeni);\n- sonra görüntü paylaştıkları yerde birleştirilir ve kaydedebildikleri her şeyle büyütülür."));

SS_MSG(desc_map_2,
    EN("--camera-model, --focal and --distortion accept one value or PREFIX=VALUE by image path.\nWithout overrides, map uses the camera setup saved by verification."),
    JA("--camera-model と --focal と --distortion は、データセット全体に効く値のほか、画像パスでカメラグループを 1 つ指定する PREFIX=VALUE の形も取れるため、リグごとにモデルを混在させられます。\nカメラについて何も指定しなければ、検証が記録した構成がそのまま使われます。"),
    ZH_HANS("--camera-model、--focal 与 --distortion 既可给出作用于整个数据集的值，也可写成 PREFIX=VALUE，按图像路径指定某一个相机组，因此同一套设备可以混用不同模型。\n若完全不提相机，就直接沿用验证阶段记录下来的配置。"),
    ZH_HANT("--camera-model、--focal 與 --distortion 既可給出作用於整個資料集的值，也可寫成 PREFIX=VALUE，按影像路徑指定某一個相機群組，因此同一套設備可以混用不同模型。\n若完全不提相機，就直接沿用驗證階段記錄下來的組態。"),
    KO("--camera-model 과 --focal 과 --distortion 은 데이터셋 전체에 걸리는 값은 물론, 이미지 경로로 카메라 그룹 하나를 지목하는 PREFIX=VALUE 형태도 받으므로 한 리그에서 여러 모델을 섞을 수 있습니다.\n카메라에 대해 아무것도 말하지 않으면 검증이 기록한 구성을 그대로 씁니다."),
    DE("--camera-model, --focal und --distortion nehmen entweder einen Wert für den ganzen Datensatz oder PREFIX=VALUE, das über den Bildpfad eine Kameragruppe benennt, sodass ein Rig Modelle mischen kann.\nSagt man nichts über Kameras, wird der von der Prüfung vermerkte Aufbau unverändert verwendet."),
    FR("--camera-model, --focal et --distortion acceptent soit une valeur pour tout le jeu de données, soit PREFIX=VALUE désignant un groupe de caméras par chemin d'image, si bien qu'un rig peut mêler des modèles.\nNe rien dire des caméras et la configuration consignée par la vérification est reprise telle quelle."),
    ES("--camera-model, --focal y --distortion aceptan o bien un valor para todo el conjunto de datos, o bien PREFIX=VALUE que nombra un grupo de cámaras por ruta de imagen, de modo que un equipo puede mezclar modelos.\nSi no se dice nada sobre las cámaras, se usa tal cual la configuración que anotó la verificación."),
    PT("--camera-model, --focal e --distortion aceitam ou um valor para todo o conjunto de dados, ou PREFIX=VALUE que nomeia um grupo de câmeras pelo caminho da imagem, de modo que um equipamento pode misturar modelos.\nSe nada for dito sobre as câmeras, usa-se tal como está a configuração que a verificação anotou."),
    IT("--camera-model, --focal e --distortion accettano o un valore per l'intero dataset o PREFIX=VALUE che nomina un gruppo di camere tramite il percorso dell'immagine, così un rig può mescolare modelli.\nSe non si dice nulla sulle camere, si usa così com'è la configurazione annotata dalla verifica."),
    NL("--camera-model, --focal en --distortion nemen ofwel een waarde voor de hele dataset, ofwel PREFIX=VALUE dat via het beeldpad één cameragroep noemt, zodat een rig modellen kan mengen.\nZeg niets over camera's en de door de verificatie genoteerde opzet wordt onveranderd gebruikt."),
    RU("--camera-model, --focal и --distortion принимают либо значение на весь набор данных, либо PREFIX=VALUE, называющее одну группу камер по пути изображения, так что установка может смешивать модели.\nНичего не сказав о камерах, вы получите тот состав, что записала проверка, как есть."),
    TR("--camera-model, --focal ve --distortion, ya tüm veri kümesi için bir değer alır ya da görüntü yoluyla tek bir kamera grubunu adlandıran PREFIX=VALUE biçimini alır;\n- böylece bir düzenek modelleri karıştırabilir.\n- Kameralar hakkında hiçbir şey söylenmezse, doğrulamanın kaydettiği düzen olduğu gibi kullanılır."));

SS_MSG(desc_merge_1,
    EN("Merge models by shared images, which determine the similarity transform between their frames.\n- A directory with 0/, 1/, ... loads all models.\n- Models must use the same features."),
    JA("共通画像から座標系間の相似変換を求めてモデルを結合。\n- 0/、1/、...を持つフォルダは全モデルを読み込みます。\n- 同じ特徴点を使ったモデルである必要があります。"),
    ZH_HANS("通过共有图像求坐标系间的相似变换，合并模型。\n- 含 0/、1/、... 的目录会加载全部模型。\n- 模型必须使用相同的特征。"),
    ZH_HANT("透過共有影像求座標系間的相似變換，合併模型。\n- 含 0/、1/、... 的目錄會載入全部模型。\n- 模型必須使用相同的特徵。"),
    KO("공통 이미지로 좌표계 간 유사 변환을 구해 모델을 병합합니다.\n- 0/, 1/, ... 폴더는 모든 모델을 읽습니다.\n- 모델은 같은 특징을 사용해야 합니다."),
    DE("Modelle über gemeinsame Bilder und die daraus bestimmte Ähnlichkeitstransformation zusammenführen.\n- Ein Ordner mit 0/, 1/, ... lädt alle Modelle.\n- Modelle müssen dieselben Features nutzen."),
    FR("Fusionne via les images communes qui fixent la similitude entre repères.\n- Un dossier avec 0/, 1/, ... charge tous les modèles.\n- Les modèles doivent utiliser les mêmes points caractéristiques."),
    ES("Fusiona por imágenes comunes, que determinan la transformación entre marcos.\n- Una carpeta con 0/, 1/, ... carga todos los modelos.\n- Deben usar los mismos rasgos."),
    PT("Funde pelas imagens comuns, que definem a transformação entre referenciais.\n- Uma pasta com 0/, 1/, ... carrega todos os modelos.\n- Devem usar as mesmas características."),
    IT("Fonde tramite immagini comuni, che definiscono la similitudine tra riferimenti.\n- Una cartella con 0/, 1/, ... carica tutti i modelli.\n- Devono usare le stesse feature."),
    NL("Voeg samen via gedeelde beelden die de gelijkvormigheid tussen frames bepalen.\n- Een map met 0/, 1/, ... laadt alle modellen.\n- Modellen moeten dezelfde kenmerken gebruiken."),
    RU("Сливает по общим снимкам, определяющим подобие между системами координат.\n- Папка с 0/, 1/, ... загружает все модели.\n- Модели должны использовать одинаковые признаки."),
    TR("Ortak görüntülerin belirlediği benzerlik dönüşümüyle modelleri birleştirir.\n- 0/, 1/, ... içeren klasör tüm modelleri yükler.\n- Modeller aynı özellikleri kullanmalıdır."));

SS_MSG(desc_merge_3,
    EN("A single model is accepted only with --metric-positions or --metric-gps: "
       "there is nothing to merge, and the run only re-gauges it in metres."),
    JA("モデルが 1 つだけの場合は --metric-positions か --metric-gps を"
       "指定したときにのみ受け付けます。統合するものはなく、メートル座標系に"
       "直すだけの実行になります。"),
    ZH_HANS("只有一个模型时，仅在给出 --metric-positions 或 --metric-gps 的情况下才接受："
            "没有可合并的内容，这次运行只是把它改写为米制坐标系。"),
    ZH_HANT("只有一個模型時，僅在給出 --metric-positions 或 --metric-gps 的情況下才接受："
            "沒有可合併的內容，這次執行只是把它改寫為公尺座標系。"),
    KO("모델이 하나뿐일 때는 --metric-positions 또는 --metric-gps 를 준 경우에만 "
       "받습니다. 병합할 것이 없고, 미터 좌표계로 다시 맞추기만 합니다."),
    DE("Ein einzelnes Modell wird nur mit --metric-positions oder --metric-gps "
       "angenommen: es gibt nichts zu vereinen, der Lauf setzt es nur in Meter."),
    FR("Un modèle seul n'est accepté qu'avec --metric-positions ou --metric-gps : "
       "il n'y a rien à fusionner, la passe ne fait que le remettre en mètres."),
    ES("Un solo modelo se acepta únicamente con --metric-positions o "
       "--metric-gps: no hay nada que fusionar y la pasada solo lo pone en metros."),
    PT("Um único modelo só é aceite com --metric-positions ou --metric-gps: não "
       "há nada a fundir e a passagem apenas o volta a pôr em metros."),
    IT("Un solo modello è accettato unicamente con --metric-positions o "
       "--metric-gps: non c'è nulla da fondere, la passata lo rimette in metri."),
    NL("Eén model wordt alleen met --metric-positions of --metric-gps aanvaard: "
       "er valt niets samen te voegen, de run zet het enkel in meters."),
    RU("Одна модель принимается только с --metric-positions или --metric-gps: "
       "сливать нечего, прогон лишь переводит её в метры."),
    TR("Tek bir model yalnızca --metric-positions ya da --metric-gps ile kabul "
       "edilir: birleştirilecek bir şey yoktur, çalıştırma onu yalnızca metreye çevirir."));

SS_MSG(desc_merge_2,
    EN("Bundle adjustment runs across the merged seam to reconcile the independently optimized models, unless --no-ba is set."),
    JA("独立に最適化されたモデルを結合境界で整合させるため、--no-ba でなければバンドル調整を行います。"),
    ZH_HANS("合并后跨接缝运行光束平差，协调独立优化的模型；--no-ba 可关闭。"),
    ZH_HANT("合併後跨接縫執行光束平差，協調獨立最佳化的模型；--no-ba 可關閉。"),
    KO("독립 최적화된 모델의 접합부를 맞추기 위해 병합 후 번들 조정을 합니다. --no-ba로 끌 수 있습니다."),
    DE("Nach dem Zusammenführen gleicht ein Bündelausgleich die Naht zwischen separat optimierten Modellen ab, außer bei --no-ba."),
    FR("Un ajustement de faisceaux corrige la jonction des modèles optimisés séparément après fusion, sauf avec --no-ba."),
    ES("Tras fusionar, un ajuste de haces corrige la unión de los modelos optimizados por separado, salvo con --no-ba."),
    PT("Após fundir, um ajuste de feixes corrige a união dos modelos otimizados separadamente, salvo com --no-ba."),
    IT("Dopo la fusione, un bundle adjustment corregge la giunzione dei modelli ottimizzati separatamente, salvo --no-ba."),
    NL("Na samenvoegen corrigeert bundelaanpassing de naad tussen afzonderlijk geoptimaliseerde modellen, tenzij --no-ba."),
    RU("После слияния уравнивание согласует шов между независимо оптимизированными моделями, если не задан --no-ba."),
    TR("Birleşim sonrası demet dengelemesi ayrı optimize edilmiş modellerin dikişini düzeltir; --no-ba ile kapatılır."));

// ===========================================================================
// The options each command parses itself
// ===========================================================================

SS_MSG(opt_auto_output,
    EN("Workspace to write: features/, matches.bin and sparse/0.. land in it."),
    JA("書き出し先のワークスペース。features/、matches.bin、sparse/0.. がここに"
       "置かれます。"),
    ZH_HANS("要写入的工作区：features/、matches.bin 与 sparse/0.. 都放在其中。"),
    ZH_HANT("要寫入的工作區：features/、matches.bin 與 sparse/0.. 都放在其中。"),
    KO("쓸 작업 공간. features/, matches.bin, sparse/0.. 이 여기에 놓입니다."),
    DE("Arbeitsbereich zum Schreiben: features/, matches.bin und sparse/0.. "
       "landen darin."),
    FR("Espace de travail où écrire : features/, matches.bin et sparse/0.. y "
       "atterrissent."),
    ES("Espacio de trabajo donde escribir: features/, matches.bin y sparse/0.. "
       "van a parar ahí."),
    PT("Espaço de trabalho onde escrever: features/, matches.bin e sparse/0.. "
       "vão parar aí."),
    IT("Spazio di lavoro in cui scrivere: features/, matches.bin e sparse/0.. "
       "finiscono lì."),
    NL("Werkruimte om in te schrijven: features/, matches.bin en sparse/0.. "
       "komen daarin terecht."),
    RU("Рабочая папка для записи: в неё попадают features/, matches.bin и "
       "sparse/0.."),
    TR("Yazılacak çalışma alanı: features/, matches.bin ve sparse/0.. buraya "
       "iner."));

SS_MSG(opt_rig,
    EN("Images under these path prefixes with the same name are one rig frame and keep a fixed relative pose (cam0,cam1 pairs cam0/x.jpg with cam1/x.jpg).\n- Repeat for several rigs; the manifest's `rigs:` can also carry known extrinsics.\n- A `dual-fisheye=` prefix says the first two are the back-to-back lenses of one 360 camera."),
    JA("これらのパス接頭辞の下で同名の画像を 1 つのリグフレームとして、相対姿勢を固定します（cam0,cam1 なら cam0/x.jpg と cam1/x.jpg が組になります）。\n- 複数のリグには繰り返し指定します。 マニフェストの `rigs:` では既知の外部パラメータも渡せます。\n- `dual-fisheye=` を前に付けると、最初の 2 つが 1 台の 360 カメラの背中合わせのレンズであることを示します。"),
    ZH_HANS("这些路径前缀下同名的图像构成一个装置帧，保持固定的相对位姿（cam0,cam1 把 cam0/x.jpg 和 cam1/x.jpg 配对）。\n- 多个装置可重复指定； 清单的 `rigs:` 还能给出已知外参。\n- 加上 `dual-fisheye=` 前缀表示前两个是同一台 360 相机背靠背的镜头。"),
    ZH_HANT("這些路徑前綴下同名的影像構成一個裝置幀，保持固定的相對姿態（cam0,cam1 把 cam0/x.jpg 和 cam1/x.jpg 配對）。\n- 多個裝置可重複指定； 清單的 `rigs:` 還能給出已知外參。\n- 加上 `dual-fisheye=` 前綴表示前兩個是同一台 360 相機背靠背的鏡頭。"),
    KO("이 경로 접두사 아래 이름이 같은 이미지는 하나의 리그 프레임이 되어 상대 자세가 고정됩니다(cam0,cam1 은 cam0/x.jpg 와 cam1/x.jpg 를 짝지음).\n- 리그가 여럿이면 반복하세요. 매니페스트의 `rigs:` 로 알려진 외부 파라미터도 줄 수 있습니다.\n- `dual-fisheye=` 를 앞에 붙이면 처음 둘이 한 360 카메라의 등을 맞댄 렌즈임을 뜻합니다."),
    DE("Gleichnamige Bilder unter diesen Präfixen bilden einen Rig-Frame mit fester relativer Pose.\n- cam0,cam1 paart cam0/x.jpg mit cam1/x.jpg. Für weitere Rigs wiederholen.\n- Das Manifest kann bekannte Extrinsik unter rigs: angeben.\n- dual-fisheye= setzt die ersten zwei Objektive einer 360-Kamera Rücken an Rücken."),
    FR("Les images de même nom sous ces préfixes de chemin forment une image de rig à pose relative fixe (cam0,cam1 apparie cam0/x.jpg à cam1/x.jpg).\n- Répéter pour plusieurs rigs ; `rigs:` dans le manifeste peut aussi porter des extrinsèques connus.\n- Le préfixe `dual-fisheye=` indique que les deux premiers sont les objectifs dos à dos d'une caméra 360."),
    ES("Las imágenes con el mismo nombre bajo estos prefijos de ruta forman un cuadro de rig con pose relativa fija (cam0,cam1 empareja cam0/x.jpg con cam1/x.jpg).\n- Repetir para varios rigs; `rigs:` en el manifiesto puede llevar también extrínsecos conocidos.\n- El prefijo `dual-fisheye=` indica que los dos primeros son las lentes espalda con espalda de una cámara 360."),
    PT("Imagens com o mesmo nome sob estes prefixos de caminho formam um quadro de rig com pose relativa fixa (cam0,cam1 emparelha cam0/x.jpg com cam1/x.jpg).\n- Repita para vários rigs; `rigs:` no manifesto também pode trazer extrínsecos conhecidos.\n- O prefixo `dual-fisheye=` diz que os dois primeiros são as lentes costas com costas de uma câmera 360."),
    IT("Le immagini con lo stesso nome sotto questi prefissi di percorso formano un fotogramma di rig a posa relativa fissa (cam0,cam1 accoppia cam0/x.jpg con cam1/x.jpg).\n- Ripetere per più rig; `rigs:` nel manifesto può anche portare estrinseci noti.\n- Il prefisso `dual-fisheye=` dice che i primi due sono gli obiettivi schiena contro schiena di una fotocamera 360."),
    NL("Beelden met dezelfde naam onder deze padvoorvoegsels vormen één rigframe met vaste relatieve pose (cam0,cam1 koppelt cam0/x.jpg aan cam1/x.jpg).\n- Herhaal voor meerdere rigs; `rigs:` in het manifest kan ook bekende extrinsieken bevatten.\n- Het voorvoegsel `dual-fisheye=` zegt dat de eerste twee de rug-aan-rug-lenzen van één 360-camera zijn."),
    RU("Изображения с одинаковым именем под этими префиксами пути образуют один кадр рига с фиксированной относительной позой (cam0,cam1 объединяет cam0/x.jpg и cam1/x.jpg).\n- Повторите для нескольких ригов; `rigs:` в манифесте может также нести известные экстринсики.\n- Префикс `dual-fisheye=` означает, что первые два -- расположенные спиной к спине объективы одной 360-камеры."),
    TR("Bu yol öneklerinin altındaki aynı adlı görüntüler sabit göreli duruşlu tek bir rig karesi olur (cam0,cam1, cam0/x.jpg ile cam1/x.jpg'yi eşler).\n- Birden çok rig için yineleyin; bildirimdeki `rigs:` bilinen dış parametreleri de taşıyabilir.\n- `dual-fisheye=` öneki, ilk ikisinin tek bir 360 kameranın sırt sırta mercekleri olduğunu söyler."));

SS_MSG(opt_manifest,
    EN("YAML/JSON capture file: images, masks, camera groups/lenses/focals, rigs, and videos with IMU/GPS. Explicit command-line flags override it."),
    JA("YAML/JSON撮影記述：画像、マスク、カメラ群・レンズ・焦点距離、リグ、IMU/GPS付き動画。コマンドラインの明示指定が優先。"),
    ZH_HANS("YAML/JSON 拍摄描述：图像、蒙版、相机分组/镜头/焦距、刚性相机组及含 IMU/GPS 的视频。显式命令行参数优先。"),
    ZH_HANT("YAML/JSON 拍攝描述：影像、遮罩、相機分組/鏡頭/焦距、剛性相機組及含 IMU/GPS 的影片。明確指定的命令列參數優先。"),
    KO("YAML/JSON 촬영 파일: 이미지, 마스크, 카메라 그룹·렌즈·초점거리, 리그, IMU/GPS 동영상. 명시적 명령줄 옵션이 우선입니다."),
    DE("YAML/JSON-Aufnahmebeschreibung: Bilder, Masken, Kameragruppen/Objektive/Brennweiten, Rigs, Videos mit IMU/GPS. Explizite CLI-Optionen gehen vor."),
    FR("Fichier YAML/JSON : images, masques, groupes/lentilles/focales, rigs, vidéos IMU/GPS. Les options explicites en ligne de commande priment."),
    ES("Archivo YAML/JSON: imágenes, máscaras, grupos/lentes/focales, rigs y vídeos IMU/GPS. Las opciones explícitas de consola prevalecen."),
    PT("Arquivo YAML/JSON: imagens, máscaras, grupos/lentes/focais, rigs e vídeos IMU/GPS. Opções explícitas de linha de comando têm prioridade."),
    IT("File YAML/JSON: immagini, maschere, gruppi/lenti/focali, rig e video IMU/GPS. Le opzioni esplicite da riga di comando hanno priorità."),
    NL("YAML/JSON-opnamebestand: beelden, maskers, cameragroepen/lenzen/brandpunten, rigs, video’s met IMU/GPS. Expliciete CLI-opties gaan voor."),
    RU("YAML/JSON-файл съёмки: снимки, маски, группы/объективы/фокусные, риги и видео с IMU/GPS. Явные флаги командной строки приоритетнее."),
    TR("YAML/JSON çekim dosyası: görüntü, maske, kamera grubu/lens/odak, rig ve IMU/GPS videoları. Açık komut satırı bayrakları önceliklidir."));

SS_MSG(opt_progress_dir,
    EN("Write current model and pair-matrix snapshots to DIR for a watching front end. Default: off; the GUI enables it for its child process."),
    JA("監視する画面向けに現在のモデルとペア行列をDIRへ書き出します。既定は無効。GUIは子プロセスに指定します。"),
    ZH_HANS("将当前模型和图像对矩阵快照写入 DIR，供前端监控。默认关闭；GUI 会为子进程启用。"),
    ZH_HANT("將目前模型和影像對矩陣快照寫入 DIR，供前端監控。預設關閉；GUI 會為子程序啟用。"),
    KO("현재 모델과 쌍 행렬 스냅샷을 DIR에 써 프런트엔드에서 봅니다. 기본은 끄기이며 GUI는 자식 프로세스에 켭니다."),
    DE("Aktuelles Modell und Paarmatrix als Momentaufnahmen nach DIR für eine Oberfläche schreiben. Standard: aus; GUI aktiviert es im Kindprozess."),
    FR("Écrit modèle courant et matrice des paires dans DIR pour suivi par une interface. Désactivé par défaut ; la GUI l’active pour son enfant."),
    ES("Escribe modelo actual y matriz de pares en DIR para una interfaz de seguimiento. Por defecto no; la GUI lo activa para su proceso hijo."),
    PT("Grava modelo atual e matriz de pares em DIR para uma interface de acompanhamento. Padrão: desligado; a GUI ativa para o processo filho."),
    IT("Scrive modello e matrice delle coppie in DIR per un’interfaccia di monitoraggio. Default: spento; la GUI lo attiva per il processo figlio."),
    NL("Schrijft huidig model en parenmatrix naar DIR voor een meekijkende interface. Standaard uit; de GUI activeert dit voor het kindproces."),
    RU("Пишет снимки модели и матрицы пар в DIR для наблюдающего интерфейса. По умолчанию выкл.; GUI включает для дочернего процесса."),
    TR("İzleyen arayüz için güncel model ve çift matrisi anlık görüntülerini DIR’e yazar. Varsayılan kapalı; GUI alt süreç için açar."));

SS_MSG(opt_no_masks,
    EN("Ignore a masks directory even if one is sitting beside the images."),
    JA("画像の隣にマスクのディレクトリがあっても無視します。"),
    ZH_HANS("即使图像旁边就有掩码目录，也一概忽略。"),
    ZH_HANT("即使影像旁邊就有遮罩目錄，也一概忽略。"),
    KO("이미지 옆에 마스크 디렉터리가 있어도 무시합니다."),
    DE("Ein Maskenverzeichnis übergehen, selbst wenn eines neben den Bildern "
       "liegt."),
    FR("Ignorer un dossier de masques même s'il se trouve à côté des images."),
    ES("Pasar por alto una carpeta de máscaras aunque esté junto a las imágenes."),
    PT("Ignorar uma pasta de máscaras mesmo que esteja ao lado das imagens."),
    IT("Ignorare una cartella di maschere anche se sta accanto alle immagini."),
    NL("Een maskermap negeren, ook als er een naast de beelden staat."),
    RU("Не обращать внимания на каталог масок, даже если он лежит рядом с "
       "изображениями."),
    TR("Görüntülerin yanında dursa bile bir maske dizinini yok say."));

SS_MSG(opt_no_manage_auto,
    EN("Skip merging, growing, reseeding and splitting, keeping the mapper's raw "
       "models."),
    JA("統合・成長・再シード・分割を省き、マッパーの生のモデルをそのまま残します。"),
    ZH_HANS("跳过合并、增长、重新选种与拆分，保留建图器输出的原始模型。"),
    ZH_HANT("略過合併、增長、重新選種與拆分，保留建圖器輸出的原始模型。"),
    KO("병합·성장·재시드·분할을 건너뛰고 매퍼의 날 모델을 그대로 둡니다."),
    DE("Zusammenführen, Wachsen, Neusäen und Teilen überspringen und die rohen "
       "Modelle des Kartierers behalten."),
    FR("Sauter la fusion, la croissance, le réamorçage et la scission, en "
       "gardant les modèles bruts du cartographe."),
    ES("Saltarse la fusión, el crecimiento, el resembrado y la partición, y "
       "quedarse con los modelos en bruto del cartógrafo."),
    PT("Pular a fusão, o crescimento, a nova semeadura e a divisão, ficando com "
       "os modelos em bruto do cartógrafo."),
    IT("Saltare fusione, crescita, nuovo innesco e divisione, tenendo i modelli "
       "grezzi del cartografo."),
    NL("Samenvoegen, groeien, opnieuw zaaien en splitsen overslaan en de ruwe "
       "modellen van de kaartmaker houden."),
    RU("Пропустить слияние, рост, повторный посев и разбиение, оставив сырые "
       "модели построителя."),
    TR("Birleştirmeyi, büyütmeyi, yeniden tohumlamayı ve bölmeyi atla; "
       "haritalayıcının ham modellerini olduğu gibi bırak."));

SS_MSG(opt_extract_output,
    EN("Output directory for a directory of images, or the .bin file for a "
       "single image."),
    JA("画像ディレクトリを渡した場合は出力ディレクトリ、1 枚の画像を渡した場合は "
       ".bin ファイル。"),
    ZH_HANS("给的是图像目录时为输出目录；给的是单张图像时为 .bin 文件。"),
    ZH_HANT("給的是影像目錄時為輸出目錄；給的是單張影像時為 .bin 檔案。"),
    KO("이미지 디렉터리를 주면 출력 디렉터리, 이미지 하나를 주면 .bin 파일."),
    DE("Ausgabeverzeichnis bei einem Bildverzeichnis, oder die .bin-Datei bei "
       "einem einzelnen Bild."),
    FR("Dossier de sortie pour un dossier d'images, ou le fichier .bin pour une "
       "seule image."),
    ES("Carpeta de salida si se da una carpeta de imágenes, o el archivo .bin si "
       "se da una sola imagen."),
    PT("Pasta de saída para uma pasta de imagens, ou o arquivo .bin para uma só "
       "imagem."),
    IT("Cartella di uscita per una cartella di immagini, o il file .bin per una "
       "sola immagine."),
    NL("Uitvoermap bij een beeldmap, of het .bin-bestand bij één beeld."),
    RU("Каталог вывода для каталога изображений или файл .bin для одного "
       "изображения."),
    TR("Görüntü dizini için çıktı dizini, tek görüntü için .bin dosyası."));

SS_MSG(opt_match_output,
    EN("Match database to write. Without it the run reports and writes nothing."),
    JA("書き出す対応データベース。指定しないと、実行しても何も報告せず何も"
       "書きません。"),
    ZH_HANS("要写出的匹配数据库。不给它，这次运行既不报告也不写出任何东西。"),
    ZH_HANT("要寫出的匹配資料庫。不給它，這次執行既不回報也不寫出任何東西。"),
    KO("쓸 대응 데이터베이스. 주지 않으면 실행해도 아무것도 알리지 않고 쓰지도 "
       "않습니다."),
    DE("Zuordnungsdatenbank zum Schreiben. Ohne sie meldet und schreibt der Lauf "
       "nichts."),
    FR("Base d'appariements à écrire. Sans elle, l'exécution ne rapporte ni "
       "n'écrit rien."),
    ES("Base de emparejamientos que escribir. Sin ella, la ejecución no informa "
       "ni escribe nada."),
    PT("Base de correspondências a escrever. Sem ela, a execução não relata nem "
       "escreve nada."),
    IT("Database degli abbinamenti da scrivere. Senza di esso l'esecuzione non "
       "riferisce né scrive nulla."),
    NL("Matchdatabase om te schrijven. Zonder die meldt en schrijft de run niets."),
    RU("База соответствий для записи. Без неё запуск ничего не сообщает и ничего "
       "не пишет."),
    TR("Yazılacak eşleşme veritabanı. O olmadan çalışma ne bir şey bildirir ne "
       "de yazar."));

SS_MSG(opt_map_output,
    EN("Directory to write the models into, as 0/, 1/, ... largest first."),
    JA("モデルを書き出すディレクトリ。0/、1/、... と大きい順に並べます。"),
    ZH_HANS("写出模型的目录，按 0/、1/……从大到小排列。"),
    ZH_HANT("寫出模型的目錄，按 0/、1/……從大到小排列。"),
    KO("모델을 쓸 디렉터리. 0/, 1/, ... 로 큰 것부터 놓습니다."),
    DE("Verzeichnis, in das die Modelle geschrieben werden, als 0/, 1/, ... das "
       "größte zuerst."),
    FR("Dossier où écrire les modèles, sous forme 0/, 1/, ... du plus grand au "
       "plus petit."),
    ES("Carpeta donde escribir los modelos, como 0/, 1/, ... del mayor al menor."),
    PT("Pasta onde escrever os modelos, como 0/, 1/, ... do maior para o menor."),
    IT("Cartella in cui scrivere i modelli, come 0/, 1/, ... dal più grande."),
    NL("Map om de modellen in te schrijven, als 0/, 1/, ... grootste eerst."),
    RU("Каталог для записи моделей, как 0/, 1/, ... начиная с наибольшей."),
    TR("Modellerin yazılacağı dizin: 0/, 1/, ... en büyükten başlayarak."));

SS_MSG(opt_map_audit,
    EN("Audit every pose against the correspondence graph before anything else, and re-register what the model cannot support. Use with --resume on models from elsewhere."),
    JA("何よりも先に、すべての姿勢を対応グラフに照らして監査し、モデルが支持できないものを登録し直します。\nよそから持ってきたモデルには --resume と併せて使ってください。"),
    ZH_HANS("先于一切，对照对应关系图审计每一个位姿，并把模型无法支撑的重新注册。对来自别处的模型请与 --resume 一同使用。"),
    ZH_HANT("先於一切，對照對應關係圖稽核每一個位姿，並把模型無法支撐的重新註冊。對來自別處的模型請與 --resume 一同使用。"),
    KO("무엇보다 먼저 모든 자세를 대응 그래프에 비추어 감사하고, 모델이 뒷받침하지 못하는 것을 다시 등록합니다.\n다른 곳에서 온 모델에는 --resume 과 함께 쓰세요."),
    DE("Posen zuerst am Korrespondenzgraphen prüfen und ungestützte neu registrieren. Für fremde Modelle mit --resume verwenden."),
    FR("Vérifie d’abord les poses contre le graphe et réenregistre celles non étayées. Utilisez avec --resume pour les modèles externes."),
    ES("Auditar cada pose contra el grafo de correspondencias antes que nada, y volver a registrar lo que el modelo no puede sostener. Úsese con --resume en modelos de otra procedencia."),
    PT("Verifica poses primeiro pelo grafo e registra de novo as sem suporte. Use com --resume em modelos externos."),
    IT("Verifica prima le pose sul grafo e registra di nuovo quelle senza supporto. Usa con --resume per modelli esterni."),
    NL("Voor alles elke pose toetsen aan de correspondentiegraaf en opnieuw registreren wat het model niet kan dragen.\nGebruik dit met --resume bij modellen van elders."),
    RU("Прежде всего сверить каждую позу с графом соответствий и перерегистрировать то, что модель не выдерживает.\nПрименяйте вместе с --resume к моделям со стороны."),
    TR("Her şeyden önce her duruşu karşılık çizgesine göre denetle ve modelin destekleyemediğini yeniden kaydet.\nBaşka yerden gelen modellerde --resume ile birlikte kullanın."));

SS_MSG(opt_no_manage_map,
    EN("Skip merging, growing, reseeding and splitting."),
    JA("統合・成長・再シード・分割を省きます。"),
    ZH_HANS("跳过合并、增长、重新选种与拆分。"),
    ZH_HANT("略過合併、增長、重新選種與拆分。"),
    KO("병합·성장·재시드·분할을 건너뜁니다."),
    DE("Zusammenführen, Wachsen, Neusäen und Teilen überspringen."),
    FR("Sauter la fusion, la croissance, le réamorçage et la scission."),
    ES("Saltarse la fusión, el crecimiento, el resembrado y la partición."),
    PT("Pular a fusão, o crescimento, a nova semeadura e a divisão."),
    IT("Saltare fusione, crescita, nuovo innesco e divisione."),
    NL("Samenvoegen, groeien, opnieuw zaaien en splitsen overslaan."),
    RU("Пропустить слияние, рост, повторный посев и разбиение."),
    TR("Birleştirmeyi, büyütmeyi, yeniden tohumlamayı ve bölmeyi atla."));

SS_MSG(opt_merge_output,
    EN("Directory to write the merged models into. Required unless --in-place."),
    JA("統合したモデルを書き出すディレクトリ。--in-place を使わない限り必須です。"),
    ZH_HANS("写出合并后模型的目录。除非使用 --in-place，否则必填。"),
    ZH_HANT("寫出合併後模型的目錄。除非使用 --in-place，否則必填。"),
    KO("병합한 모델을 쓸 디렉터리. --in-place 를 쓰지 않는 한 필수입니다."),
    DE("Verzeichnis, in das die vereinten Modelle geschrieben werden. "
       "Erforderlich, sofern nicht --in-place."),
    FR("Dossier où écrire les modèles fusionnés. Obligatoire sauf avec "
       "--in-place."),
    ES("Carpeta donde escribir los modelos fusionados. Obligatoria salvo con "
       "--in-place."),
    PT("Pasta onde escrever os modelos fundidos. Obrigatória salvo com "
       "--in-place."),
    IT("Cartella in cui scrivere i modelli fusi. Obbligatoria salvo con "
       "--in-place."),
    NL("Map om de samengevoegde modellen in te schrijven. Verplicht tenzij "
       "--in-place."),
    RU("Каталог для записи слитых моделей. Обязателен, если не задан --in-place."),
    TR("Birleştirilmiş modellerin yazılacağı dizin. --in-place verilmedikçe "
       "zorunludur."));

// ===========================================================================
// `auto`'s exit status
// ===========================================================================

SS_MSG(exit_0,
    EN("a reconstruction that looks sound"),
    JA("健全に見える再構成"),
    ZH_HANS("看起来站得住的重建"),
    ZH_HANT("看起來站得住的重建"),
    KO("건전해 보이는 재구성"),
    DE("eine Rekonstruktion, die stimmig aussieht"),
    FR("une reconstruction qui paraît saine"),
    ES("una reconstrucción que parece sólida"),
    PT("uma reconstrução que parece sólida"),
    IT("una ricostruzione che sembra solida"),
    NL("een reconstructie die deugdelijk oogt"),
    RU("реконструкция, которая выглядит состоятельной"),
    TR("sağlam görünen bir yeniden oluşturma"));

SS_MSG(exit_1,
    EN("usage or runtime error"),
    JA("使い方の誤り、または実行時のエラー"),
    ZH_HANS("用法错误或运行时错误"),
    ZH_HANT("用法錯誤或執行時錯誤"),
    KO("사용법 오류 또는 실행 중 오류"),
    DE("Aufruf- oder Laufzeitfehler"),
    FR("erreur d'utilisation ou d'exécution"),
    ES("error de uso o de ejecución"),
    PT("erro de uso ou de execução"),
    IT("errore d'uso o di esecuzione"),
    NL("gebruiks- of uitvoeringsfout"),
    RU("ошибка вызова или выполнения"),
    TR("kullanım ya da çalışma zamanı hatası"));

SS_MSG(exit_2,
    EN("no reconstruction at all"),
    JA("再構成がまったく得られなかった"),
    ZH_HANS("完全没有得到重建"),
    ZH_HANT("完全沒有得到重建"),
    KO("재구성이 전혀 나오지 않음"),
    DE("überhaupt keine Rekonstruktion"),
    FR("aucune reconstruction du tout"),
    ES("ninguna reconstrucción en absoluto"),
    PT("nenhuma reconstrução de todo"),
    IT("nessuna ricostruzione affatto"),
    NL("helemaal geen reconstructie"),
    RU("реконструкции нет вовсе"),
    TR("hiç yeniden oluşturma yok"));

SS_MSG(exit_3,
    EN("partial: under half the images registered, or over 2 px mean "
       "reprojection"),
    JA("部分的: 登録された画像が半数未満、または平均再投影誤差が 2 px 超"),
    ZH_HANS("只完成一部分：注册的图像不到一半，或平均重投影误差超过 2 px"),
    ZH_HANT("只完成一部分：註冊的影像不到一半，或平均重投影誤差超過 2 px"),
    KO("일부만: 등록된 이미지가 절반 미만이거나 평균 재투영 오차가 2 px 초과"),
    DE("teilweise: weniger als die Hälfte der Bilder registriert, oder über 2 px "
       "mittlere Rückprojektion"),
    FR("partiel : moins de la moitié des images enregistrées, ou plus de 2 px de "
       "reprojection moyenne"),
    ES("parcial: menos de la mitad de las imágenes registradas, o más de 2 px de "
       "reproyección media"),
    PT("parcial: menos de metade das imagens registradas, ou mais de 2 px de "
       "reprojeção média"),
    IT("parziale: meno della metà delle immagini registrate, o oltre 2 px di "
       "riproiezione media"),
    NL("gedeeltelijk: minder dan de helft van de beelden geregistreerd, of meer "
       "dan 2 px gemiddelde herprojectie"),
    RU("частично: зарегистрировано меньше половины изображений либо средняя "
       "репроекция выше 2 px"),
    TR("kısmi: görüntülerin yarısından azı kaydedildi ya da ortalama yeniden "
       "izdüşüm 2 px'in üzerinde"));


// ===========================================================================
// `spirula sfm ba` -- one global bundle adjustment of a model
// ===========================================================================

SS_MSG(exit_4,
    EN("the model was written, but not in the metric frame that was asked for"),
    JA("モデルは書き出されましたが、要求されたメートル座標系ではありません"),
    ZH_HANS("模型已写出，但不是所要求的米制坐标系"),
    ZH_HANT("模型已寫出，但不是所要求的公尺座標系"),
    KO("모델은 썼지만 요청한 미터 좌표계는 아닙니다"),
    DE("das Modell wurde geschrieben, aber nicht im verlangten metrischen Rahmen"),
    FR("le modèle a été écrit, mais pas dans le repère métrique demandé"),
    ES("el modelo se escribió, pero no en el marco métrico solicitado"),
    PT("o modelo foi escrito, mas não no referencial métrico pedido"),
    IT("il modello è stato scritto, ma non nel sistema metrico richiesto"),
    NL("het model is geschreven, maar niet in het gevraagde metrische stelsel"),
    RU("модель записана, но не в запрошенной метрической системе"),
    TR("model yazıldı, ancak istenen metrik çerçevede değil"));

SS_MSG(ba_desc_1,
    EN("Refine a COLMAP sparse model with global bundle adjustment. OUTPUT may be the input directory.\n- Default loss: Huber 2 px. Change it with --loss.\n- Uses the model’s rigs.txt unless --rig overrides it.\n- If the GPU runs out of memory or loses the device, the CPU resumes from the last GPU checkpoint."),
    JA("COLMAP の疎なモデルを読み込み、マッパーが行うのと同じ全体バンドル調整を実行して（--loss の指定がなければ Huber 2 px、--rig で別のリグを指定しなければモデルの rigs.txt のリグを使用）、精密化したモデルを OUTPUT に書き出します。\n- OUTPUT は入力ディレクトリそのものでも構いません。\n- GPU が途中で失敗した場合（VK_ERROR_DEVICE_LOST やメモリ不足）は、GPU が最後に保存したチェックポイントから CPU が続きを実行します。"),
    ZH_HANS("读取 COLMAP 稀疏模型，对其执行与建图器相同的全局平差（除非 --loss 另有指定，否则用 Huber 2 px；除非 --rig 指定了别的装配，否则使用模型 rigs.txt 中的装配），并把精化后的模型写到 OUTPUT，OUTPUT 可以就是输入目录本身。\n若 GPU 中途失败（VK_ERROR_DEVICE_LOST 或显存不足），CPU 会从 GPU 最后保存的检查点接着算。"),
    ZH_HANT("讀取 COLMAP 稀疏模型，對其執行與建圖器相同的全域平差（除非 --loss 另有指定，否則用 Huber 2 px；除非 --rig 指定了別的裝配，否則使用模型 rigs.txt 中的裝配），並把精化後的模型寫到 OUTPUT，OUTPUT 可以就是輸入目錄本身。\n若 GPU 中途失敗（VK_ERROR_DEVICE_LOST 或顯示記憶體不足），CPU 會從 GPU 最後儲存的檢查點接著算。"),
    KO("COLMAP 성긴 모델을 읽어 매퍼가 돌리는 것과 같은 전역 번들 조정을 실행하고(--loss 로 달리 말하지 않으면 Huber 2 px, --rig 로 다른 리그를 지정하지 않으면 모델의 rigs.txt 에 있는 리그 사용), 정련된 모델을 OUTPUT 에 씁니다.\nOUTPUT 은 입력 디렉터리 자체여도 됩니다. GPU 가 도중에 실패하면(VK_ERROR_DEVICE_LOST 또는 메모리 부족) GPU 가 마지막으로 저장한 체크포인트부터 CPU 가 이어서 계산합니다."),
    DE("Liest ein dünnes COLMAP-Modell, führt darauf den globalen Bündelausgleich aus, den der Kartierer ausführt (Huber 2 px, sofern --loss nichts anderes sagt, mit den Rigs aus der rigs.txt des Modells, sofern --rig keine anderen nennt), und schreibt das verfeinerte Modell nach OUTPUT, das auch das Eingabeverzeichnis selbst sein darf.\nScheitert die GPU unterwegs (VK_ERROR_DEVICE_LOST oder Speichermangel), rechnet die CPU ab dem letzten Prüfpunkt weiter, den die GPU gesichert hat."),
    FR("Lit un modèle épars COLMAP, y exécute l'ajustement de faisceaux global que le cartographe exécute (Huber 2 px sauf indication de --loss, avec les rigs du rigs.txt du modèle sauf si --rig en nomme d'autres), et écrit le modèle affiné dans OUTPUT, qui peut être le dossier d'entrée lui-même.\nSi le GPU échoue en cours de route (VK_ERROR_DEVICE_LOST, ou mémoire épuisée), le CPU reprend au dernier point de contrôle sauvegardé par le GPU."),
    ES("Lee un modelo disperso de COLMAP, ejecuta sobre él el ajuste de haces global que ejecuta el cartógrafo (Huber 2 px salvo que --loss diga otra cosa, con los rigs del rigs.txt del modelo salvo que --rig nombre otros) y escribe el modelo refinado en OUTPUT, que puede ser la propia carpeta de entrada.\nSi la GPU falla a mitad (VK_ERROR_DEVICE_LOST, o sin memoria), la CPU continúa desde el último punto de control que guardó la GPU."),
    PT("Lê um modelo esparso do COLMAP, executa sobre ele o ajuste de feixes global que o cartógrafo executa (Huber 2 px salvo se --loss disser outra coisa, com os rigs do rigs.txt do modelo salvo se --rig nomear outros) e escreve o modelo refinado em OUTPUT, que pode ser a própria pasta de entrada.\nSe a GPU falhar a meio (VK_ERROR_DEVICE_LOST, ou sem memória), a CPU continua a partir do último ponto de controle que a GPU guardou."),
    IT("Legge un modello sparso COLMAP, vi esegue il bundle adjustment globale che esegue il cartografo (Huber 2 px salvo diverso --loss, con i rig del rigs.txt del modello salvo che --rig ne indichi altri) e scrive il modello affinato in OUTPUT, che può essere la cartella di ingresso stessa.\nSe la GPU fallisce a metà (VK_ERROR_DEVICE_LOST, o memoria esaurita), la CPU prosegue dall'ultimo punto di controllo salvato dalla GPU."),
    NL("Leest een ijl COLMAP-model, draait daarop de globale bundelaanpassing die de kaartmaker draait (Huber 2 px tenzij --loss anders zegt, met de rigs uit de rigs.txt van het model tenzij --rig andere noemt), en schrijft het verfijnde model naar OUTPUT, dat ook de invoermap zelf mag zijn.\nFaalt de GPU halverwege (VK_ERROR_DEVICE_LOST, of geheugen op), dan rekent de CPU verder vanaf het laatste controlepunt dat de GPU bewaarde."),
    RU("Читает разреженную модель COLMAP, выполняет на ней то глобальное уравнивание, которое выполняет картограф (Huber 2 px, если --loss не задаёт иное, с ригами из rigs.txt модели, если --rig не называет другие), и записывает уточнённую модель в OUTPUT — это может быть и сам входной каталог.\nЕсли GPU сбоит посреди работы (VK_ERROR_DEVICE_LOST или нехватка памяти), CPU продолжает с последней контрольной точки, сохранённой GPU."),
    TR("Bir COLMAP seyrek modelini okur, üzerinde haritalayıcının çalıştırdığı genel demet dengelemesini çalıştırır (--loss aksini söylemedikçe Huber 2 px; --rig başkalarını belirtmedikçe modelin rigs.txt dosyasındaki düzeneklerle) ve iyileştirilmiş modeli OUTPUT'a yazar;\n- OUTPUT girdi dizininin kendisi de olabilir.\n- GPU yarı yolda başarısız olursa (VK_ERROR_DEVICE_LOST ya da bellek yetersizliği), CPU GPU'nun kaydettiği son denetim noktasından devam eder."));

SS_MSG(ba_desc_2,
    EN("For a Bundle Adjustment in the Large (BAL) problem, refine it and write points as PLY to OUTPUT. Used for solver benchmarks, not by the pipeline."),
    JA("Bundle Adjustment in the Large（BAL）問題を最適化し、点群をPLYでOUTPUTへ書きます。ソルバーベンチマーク用で、パイプラインでは使いません。"),
    ZH_HANS("对 Bundle Adjustment in the Large（BAL）问题优化，将点以 PLY 写入 OUTPUT。用于求解器基准测试，流水线不使用 BAL。"),
    ZH_HANT("對 Bundle Adjustment in the Large（BAL）問題最佳化，將點以 PLY 寫入 OUTPUT。用於求解器基準測試，流程不使用 BAL。"),
    KO("Bundle Adjustment in the Large(BAL) 문제를 최적화해 점을 PLY로 OUTPUT에 씁니다. 솔버 벤치마크용이며 파이프라인은 쓰지 않습니다."),
    DE("Bundle Adjustment in the Large (BAL) lösen und verfeinerte Punkte als PLY nach OUTPUT schreiben. Für Solverbenchmarks, nicht die Pipeline."),
    FR("Optimise un problème Bundle Adjustment in the Large (BAL), écrit les points en PLY dans OUTPUT. Pour benchmarks, pas pour le pipeline."),
    ES("Optimiza un problema Bundle Adjustment in the Large (BAL) y escribe puntos PLY en OUTPUT. Para pruebas del solver, no para la canalización."),
    PT("Otimiza um problema Bundle Adjustment in the Large (BAL) e grava pontos PLY em OUTPUT. Para testes do solver, não para o pipeline."),
    IT("Ottimizza un problema Bundle Adjustment in the Large (BAL) e scrive punti PLY in OUTPUT. Per benchmark del solver, non per la pipeline."),
    NL("Verfijn een Bundle Adjustment in the Large (BAL)-probleem en schrijf punten als PLY naar OUTPUT. Voor solvertests, niet voor de pijplijn."),
    RU("Уточняет задачу Bundle Adjustment in the Large (BAL), пишет точки PLY в OUTPUT. Для тестов решателя, не для конвейера."),
    TR("Bundle Adjustment in the Large (BAL) problemini iyileştirip noktaları PLY olarak OUTPUT’a yazar. Çözücü testi içindir, işlem hattı kullanmaz."));

SS_MSG(ba_note,
    EN("A (--real, --loss) pair that was trimmed out of the build reports "
       "\"variant not built into this binary\"; see SS_SFM_REALS / "
       "SS_SFM_LOSSES."),
    JA("ビルドから外された (--real, --loss) の組み合わせは "
       "\"variant not built into this binary\" と報告します。"
       "SS_SFM_REALS / SS_SFM_LOSSES を参照してください。"),
    ZH_HANS("被从构建中裁掉的 (--real, --loss) 组合会报告 "
            "\"variant not built into this binary\"；参见 SS_SFM_REALS / "
            "SS_SFM_LOSSES。"),
    ZH_HANT("被從建置中裁掉的 (--real, --loss) 組合會回報 "
            "\"variant not built into this binary\"；參見 SS_SFM_REALS / "
            "SS_SFM_LOSSES。"),
    KO("빌드에서 잘려 나간 (--real, --loss) 조합은 "
       "\"variant not built into this binary\" 라고 알립니다. "
       "SS_SFM_REALS / SS_SFM_LOSSES 를 보세요."),
    DE("Ein (--real, --loss)-Paar, das aus dem Build herausgeschnitten wurde, "
       "meldet \"variant not built into this binary\"; siehe SS_SFM_REALS / "
       "SS_SFM_LOSSES."),
    FR("Un couple (--real, --loss) retiré de la compilation signale \"variant "
       "not built into this binary\" ; voir SS_SFM_REALS / SS_SFM_LOSSES."),
    ES("Un par (--real, --loss) que se recortó de la compilación informa "
       "\"variant not built into this binary\"; véase SS_SFM_REALS / "
       "SS_SFM_LOSSES."),
    PT("Um par (--real, --loss) que foi cortado da compilação relata \"variant "
       "not built into this binary\"; veja SS_SFM_REALS / SS_SFM_LOSSES."),
    IT("Una coppia (--real, --loss) tolta dalla build riferisce \"variant not "
       "built into this binary\"; veda SS_SFM_REALS / SS_SFM_LOSSES."),
    NL("Een (--real, --loss)-paar dat uit de build is geknipt meldt \"variant "
       "not built into this binary\"; zie SS_SFM_REALS / SS_SFM_LOSSES."),
    RU("Пара (--real, --loss), вырезанная из сборки, сообщает \"variant not "
       "built into this binary\"; см. SS_SFM_REALS / SS_SFM_LOSSES."),
    TR("Derlemeden çıkarılmış bir (--real, --loss) çifti \"variant not built "
       "into this binary\" bildirir; bkz. SS_SFM_REALS / SS_SFM_LOSSES."));

SS_MSG(ba_opt_real,
    EN("scalar the solver works in; df is an emulated double-float, for devices "
       "without fp64"),
    JA("ソルバーが計算に使うスカラー型。df は fp64 を持たないデバイス向けに"
       "double を float 2 つで模擬したものです"),
    ZH_HANS("求解器所用的标量类型；df 是用两个 float 模拟的双精度，面向没有 fp64 的设备"),
    ZH_HANT("求解器所用的純量型別；df 是用兩個 float 模擬的雙精度，面向沒有 fp64 的裝置"),
    KO("솔버가 계산에 쓰는 스칼라 형. df 는 fp64 가 없는 장치를 위해 float 두 개로 "
       "흉내 낸 배정밀도입니다"),
    DE("Skalar, in dem der Löser rechnet; df ist ein emuliertes Double-Float für "
       "Geräte ohne fp64"),
    FR("scalaire dans lequel travaille le solveur ; df est un double-float émulé, "
       "pour les appareils sans fp64"),
    ES("escalar con el que trabaja el solucionador; df es un doble-flotante "
       "emulado, para dispositivos sin fp64"),
    PT("escalar em que o solucionador trabalha; df é um duplo-flutuante emulado, "
       "para dispositivos sem fp64"),
    IT("scalare in cui lavora il risolutore; df è un double-float emulato, per "
       "dispositivi senza fp64"),
    NL("scalair waarin de oplosser werkt; df is een nagebootste double-float, "
       "voor apparaten zonder fp64"),
    RU("скаляр, в котором работает решатель; df -- эмулированный double-float "
       "для устройств без fp64"),
    TR("çözücünün çalıştığı skaler; df, fp64 olmayan aygıtlar için taklit edilmiş "
       "bir double-float'tır"));

SS_MSG(ba_opt_loss,
    EN("robust loss"),
    JA("ロバスト損失"),
    ZH_HANS("稳健损失函数"),
    ZH_HANT("穩健損失函數"),
    KO("강건 손실"),
    DE("robuster Verlust"),
    FR("perte robuste"),
    ES("pérdida robusta"),
    PT("perda robusta"),
    IT("perdita robusta"),
    NL("robuust verlies"),
    RU("устойчивая функция потерь"),
    TR("gürbüz yitim"));

SS_MSG(ba_opt_loss_param,
    EN("Huber delta / Cauchy c"),
    JA("Huber の delta / Cauchy の c"),
    ZH_HANS("Huber 的 delta 或 Cauchy 的 c"),
    ZH_HANT("Huber 的 delta 或 Cauchy 的 c"),
    KO("Huber 의 delta 또는 Cauchy 의 c"),
    DE("Huber-Delta / Cauchy-c"),
    FR("delta de Huber / c de Cauchy"),
    ES("delta de Huber / c de Cauchy"),
    PT("delta de Huber / c de Cauchy"),
    IT("delta di Huber / c di Cauchy"),
    NL("Huber-delta / Cauchy-c"),
    RU("Huber delta / Cauchy c"),
    TR("Huber delta / Cauchy c"));

SS_MSG(ba_opt_model,
    EN("BAL camera model"),
    JA("BAL のカメラモデル"),
    ZH_HANS("BAL 的相机模型"),
    ZH_HANT("BAL 的相機模型"),
    KO("BAL 카메라 모델"),
    DE("BAL-Kameramodell"),
    FR("modèle de caméra BAL"),
    ES("modelo de cámara de BAL"),
    PT("modelo de câmera do BAL"),
    IT("modello di camera BAL"),
    NL("BAL-cameramodel"),
    RU("модель камеры BAL"),
    TR("BAL kamera modeli"));

SS_MSG(ba_opt_shared_intrinsics,
    EN("one intrinsics block for every camera"),
    JA("すべてのカメラで内部パラメータのブロックを 1 つに共有します"),
    ZH_HANS("所有相机共用一个内参块"),
    ZH_HANT("所有相機共用一個內參區塊"),
    KO("모든 카메라가 내부 파라미터 블록 하나를 공유합니다"),
    DE("ein Intrinsics-Block für jede Kamera"),
    FR("un seul bloc de paramètres internes pour toutes les caméras"),
    ES("un solo bloque de parámetros internos para todas las cámaras"),
    PT("um só bloco de parâmetros internos para todas as câmeras"),
    IT("un solo blocco di parametri interni per tutte le camere"),
    NL("één intrinsiekenblok voor alle camera's"),
    RU("один блок внутренних параметров на все камеры"),
    TR("tüm kameralar için tek bir iç parametre bloğu"));

SS_MSG(ba_opt_solver,
    EN("dense Cholesky or implicit-Schur PCG"),
    JA("密なコレスキー分解、または陰的シューア補元の PCG"),
    ZH_HANS("稠密 Cholesky 分解，或隐式 Schur 补的 PCG"),
    ZH_HANT("稠密 Cholesky 分解，或隱式 Schur 補的 PCG"),
    KO("조밀 촐레스키 분해 또는 암시적 슈어 보수 PCG"),
    DE("dichte Cholesky-Zerlegung oder implizites Schur-PCG"),
    FR("Cholesky dense ou PCG à complément de Schur implicite"),
    ES("Cholesky densa o PCG de complemento de Schur implícito"),
    PT("Cholesky densa ou PCG de complemento de Schur implícito"),
    IT("Cholesky densa o PCG a complemento di Schur implicito"),
    NL("dichte Cholesky of impliciete Schur-PCG"),
    RU("плотное разложение Холецкого или PCG с неявным дополнением Шура"),
    TR("yoğun Cholesky ya da örtük Schur PCG"));

SS_MSG(ba_opt_max_iters,
    EN("Levenberg-Marquardt iteration cap"),
    JA("Levenberg-Marquardt の反復回数の上限"),
    ZH_HANS("Levenberg-Marquardt 的迭代次数上限"),
    ZH_HANT("Levenberg-Marquardt 的疊代次數上限"),
    KO("Levenberg-Marquardt 반복 횟수 상한"),
    DE("Obergrenze der Levenberg-Marquardt-Iterationen"),
    FR("plafond d'itérations de Levenberg-Marquardt"),
    ES("tope de iteraciones de Levenberg-Marquardt"),
    PT("limite de iterações de Levenberg-Marquardt"),
    IT("tetto di iterazioni di Levenberg-Marquardt"),
    NL("plafond op Levenberg-Marquardt-iteraties"),
    RU("предел числа итераций Левенберга -- Марквардта"),
    TR("Levenberg-Marquardt yineleme üst sınırı"));

SS_MSG(ba_opt_damping,
    EN("initial LM damping"),
    JA("LM の初期減衰量"),
    ZH_HANS("LM 的初始阻尼"),
    ZH_HANT("LM 的初始阻尼"),
    KO("LM 초기 감쇠"),
    DE("anfängliche LM-Dämpfung"),
    FR("amortissement LM initial"),
    ES("amortiguamiento inicial de LM"),
    PT("amortecimento inicial do LM"),
    IT("smorzamento iniziale di LM"),
    NL("aanvankelijke LM-demping"),
    RU("начальное демпфирование LM"),
    TR("başlangıç LM sönümü"));

SS_MSG(ba_opt_rtol,
    EN("relative cost improvement to stop below"),
    JA("これを下回ったら停止する相対コスト改善量"),
    ZH_HANS("相对代价改善低于此值即停止"),
    ZH_HANT("相對代價改善低於此值即停止"),
    KO("이 값 아래로 떨어지면 멈추는 상대 비용 개선량"),
    DE("relative Kostenverbesserung, unter der angehalten wird"),
    FR("amélioration relative du coût sous laquelle on s'arrête"),
    ES("mejora relativa del coste por debajo de la cual se para"),
    PT("melhoria relativa do custo abaixo da qual se para"),
    IT("miglioramento relativo del costo sotto il quale ci si ferma"),
    NL("relatieve kostenverbetering waaronder gestopt wordt"),
    RU("относительное улучшение стоимости, ниже которого происходит остановка"),
    TR("altında durulacak göreli maliyet iyileşmesi"));

SS_MSG(ba_opt_patience,
    EN("steps below --rtol before stopping"),
    JA("停止するまでに --rtol を下回るステップの数"),
    ZH_HANS("停止前允许有多少步低于 --rtol"),
    ZH_HANT("停止前允許有多少步低於 --rtol"),
    KO("멈추기 전까지 --rtol 아래인 단계 수"),
    DE("Schritte unter --rtol vor dem Anhalten"),
    FR("pas sous --rtol avant l'arrêt"),
    ES("pasos por debajo de --rtol antes de parar"),
    PT("passos abaixo de --rtol antes de parar"),
    IT("passi sotto --rtol prima di fermarsi"),
    NL("stappen onder --rtol voor het stoppen"),
    RU("сколько шагов ниже --rtol до остановки"),
    TR("durmadan önce --rtol altındaki adım sayısı"));

SS_MSG(ba_opt_cg_iters,
    EN("PCG iteration cap per LM step"),
    JA("LM の 1 ステップあたりの PCG 反復回数の上限"),
    ZH_HANS("每个 LM 步的 PCG 迭代次数上限"),
    ZH_HANT("每個 LM 步的 PCG 疊代次數上限"),
    KO("LM 한 단계당 PCG 반복 횟수 상한"),
    DE("Obergrenze der PCG-Iterationen je LM-Schritt"),
    FR("plafond d'itérations PCG par pas LM"),
    ES("tope de iteraciones de PCG por paso de LM"),
    PT("limite de iterações de PCG por passo de LM"),
    IT("tetto di iterazioni PCG per passo LM"),
    NL("plafond op PCG-iteraties per LM-stap"),
    RU("предел числа итераций PCG на шаг LM"),
    TR("LM adımı başına PCG yineleme üst sınırı"));

SS_MSG(ba_opt_cg_tol,
    EN("PCG relative residual tolerance"),
    JA("PCG の相対残差の許容値"),
    ZH_HANS("PCG 的相对残差容差"),
    ZH_HANT("PCG 的相對殘差容差"),
    KO("PCG 상대 잔차 허용값"),
    DE("relative Residuentoleranz des PCG"),
    FR("tolérance de résidu relatif du PCG"),
    ES("tolerancia de residuo relativo del PCG"),
    PT("tolerância de resíduo relativo do PCG"),
    IT("tolleranza di residuo relativo del PCG"),
    NL("relatieve residutolerantie van de PCG"),
    RU("допуск относительной невязки PCG"),
    TR("PCG göreli artık toleransı"));

SS_MSG(ba_opt_cg_fallback,
    EN("fall back to dense when PCG stalls"),
    JA("PCG が停滞したら密な解法に切り替えます"),
    ZH_HANS("当 PCG 停滞时退回稠密解法"),
    ZH_HANT("當 PCG 停滯時退回稠密解法"),
    KO("PCG 가 정체되면 조밀 해법으로 물러납니다"),
    DE("auf dicht zurückfallen, wenn PCG stockt"),
    FR("repli sur le dense quand le PCG cale"),
    ES("recurrir al denso cuando el PCG se atasca"),
    PT("recorrer ao denso quando o PCG empaca"),
    IT("ripiegare sul denso quando il PCG si blocca"),
    NL("terugvallen op dicht wanneer de PCG vastloopt"),
    RU("переходить к плотному решению, когда PCG буксует"),
    TR("PCG takıldığında yoğuna dön"));

SS_MSG(ba_opt_vram_budget,
    EN("device memory the solver may use"),
    JA("ソルバーが使ってよいデバイスメモリ量"),
    ZH_HANS("求解器可使用的设备显存"),
    ZH_HANT("求解器可使用的裝置顯示記憶體"),
    KO("솔버가 써도 되는 장치 메모리"),
    DE("Gerätespeicher, den der Löser nutzen darf"),
    FR("mémoire du périphérique que le solveur peut employer"),
    ES("memoria del dispositivo que el solucionador puede usar"),
    PT("memória do dispositivo que o solucionador pode usar"),
    IT("memoria del dispositivo che il risolutore può usare"),
    NL("apparaatgeheugen dat de oplosser mag gebruiken"),
    RU("сколько памяти устройства может занять решатель"),
    TR("çözücünün kullanabileceği aygıt belleği"));

SS_MSG(ba_opt_device,
    EN("Vulkan device: index, name, auto or uuid:hex"),
    JA("Vulkan デバイス: 番号、名前、auto、uuid:hex"),
    ZH_HANS("Vulkan 设备：序号、名称、auto 或 uuid:hex"),
    ZH_HANT("Vulkan 裝置：序號、名稱、auto 或 uuid:hex"),
    KO("Vulkan 장치: 번호, 이름, auto, uuid:hex"),
    DE("Vulkan-Gerät: Index, Name, auto oder uuid:hex"),
    FR("périphérique Vulkan : indice, nom, auto ou uuid:hex"),
    ES("dispositivo Vulkan: índice, nombre, auto o uuid:hex"),
    PT("dispositivo Vulkan: índice, nome, auto ou uuid:hex"),
    IT("dispositivo Vulkan: indice, nome, auto o uuid:hex"),
    NL("Vulkan-apparaat: index, naam, auto of uuid:hex"),
    RU("устройство Vulkan: номер, имя, auto или uuid:hex"),
    TR("Vulkan aygıtı: sıra, ad, auto veya uuid:hex"));

SS_MSG(ba_opt_validate,
    EN("check the assembled system against a reference"),
    JA("組み立てた方程式系をリファレンスと突き合わせて検証します"),
    ZH_HANS("把装配好的方程组与参考实现核对"),
    ZH_HANT("把裝配好的方程組與參考實作核對"),
    KO("조립된 연립방정식을 참조 구현과 대조합니다"),
    DE("das aufgebaute System gegen eine Referenz prüfen"),
    FR("vérifier le système assemblé face à une référence"),
    ES("comprobar el sistema ensamblado frente a una referencia"),
    PT("conferir o sistema montado face a uma referência"),
    IT("controllare il sistema assemblato rispetto a un riferimento"),
    NL("het samengestelde stelsel toetsen aan een referentie"),
    RU("сверить собранную систему с эталоном"),
    TR("kurulan sistemi bir referansa karşı denetle"));

SS_MSG(ba_opt_profile,
    EN("per-kernel timings"),
    JA("カーネルごとの所要時間"),
    ZH_HANS("逐内核计时"),
    ZH_HANT("逐核心計時"),
    KO("커널별 시간"),
    DE("Zeiten je Kernel"),
    FR("temps par noyau"),
    ES("tiempos por núcleo"),
    PT("tempos por núcleo"),
    IT("tempi per kernel"),
    NL("tijden per kernel"),
    RU("время по ядрам"),
    TR("çekirdek başına süreler"));

SS_MSG(ba_opt_quiet,
    EN("only the result lines"),
    JA("結果の行だけを表示します"),
    ZH_HANS("只输出结果行"),
    ZH_HANT("只輸出結果行"),
    KO("결과 줄만 출력합니다"),
    DE("nur die Ergebniszeilen"),
    FR("seulement les lignes de résultat"),
    ES("solo las líneas de resultado"),
    PT("apenas as linhas de resultado"),
    IT("solo le righe di risultato"),
    NL("alleen de resultaatregels"),
    RU("только строки результата"),
    TR("yalnızca sonuç satırları"));

SS_MSG(ba_opt_spv_path,
    EN("load the solver kernels from this SPIR-V file"),
    JA("ソルバーのカーネルをこの SPIR-V ファイルから読み込みます"),
    ZH_HANS("从这个 SPIR-V 文件加载求解器内核"),
    ZH_HANT("從這個 SPIR-V 檔案載入求解器核心"),
    KO("솔버 커널을 이 SPIR-V 파일에서 불러옵니다"),
    DE("die Löser-Kernel aus dieser SPIR-V-Datei laden"),
    FR("charger les noyaux du solveur depuis ce fichier SPIR-V"),
    ES("cargar los núcleos del solucionador desde este archivo SPIR-V"),
    PT("carregar os núcleos do solucionador deste arquivo SPIR-V"),
    IT("caricare i kernel del risolutore da questo file SPIR-V"),
    NL("de oplosserkernels uit dit SPIR-V-bestand laden"),
    RU("загрузить ядра решателя из этого файла SPIR-V"),
    TR("çözücü çekirdeklerini bu SPIR-V dosyasından yükle"));

SS_MSG(ba_res_initial_cost,
    EN("initial cost"), JA("初期コスト"), ZH_HANS("初始代价"), ZH_HANT("初始代價"),
    KO("초기 비용"), DE("Anfangskosten"), FR("coût initial"), ES("coste inicial"),
    PT("custo inicial"), IT("costo iniziale"), NL("beginkosten"),
    RU("начальная стоимость"), TR("başlangıç maliyeti"));

SS_MSG(ba_res_final_cost,
    EN("final cost"), JA("最終コスト"), ZH_HANS("最终代价"), ZH_HANT("最終代價"),
    KO("최종 비용"), DE("Endkosten"), FR("coût final"), ES("coste final"),
    PT("custo final"), IT("costo finale"), NL("eindkosten"),
    RU("конечная стоимость"), TR("son maliyet"));

SS_MSG(ba_res_iterations,
    EN("iterations: {0} ({1} accepted)"),
    JA("反復: {0}（うち採用 {1}）"),
    ZH_HANS("迭代：{0}（接受 {1}）"),
    ZH_HANT("疊代：{0}（接受 {1}）"),
    KO("반복: {0}(받아들임 {1})"),
    DE("Iterationen: {0} ({1} angenommen)"),
    FR("itérations : {0} ({1} acceptées)"),
    ES("iteraciones: {0} ({1} aceptadas)"),
    PT("iterações: {0} ({1} aceitas)"),
    IT("iterazioni: {0} ({1} accettate)"),
    NL("iteraties: {0} ({1} aanvaard)"),
    RU("итерации: {0} (принято {1})"),
    TR("yineleme: {0} ({1} kabul edildi)"));

SS_MSG(ba_res_cg,
    EN("cg: {0} iterations per solve on average, dense fallbacks: {1}"),
    JA("cg: 1 回の解法あたり平均 {0} 反復、密解法への切り替え: {1}"),
    ZH_HANS("cg：每次求解平均 {0} 次迭代，退回稠密解法：{1}"),
    ZH_HANT("cg：每次求解平均 {0} 次疊代，退回稠密解法：{1}"),
    KO("cg: 한 번 풀 때 평균 {0}회 반복, 조밀 해법으로 물러남: {1}"),
    DE("cg: {0} Iterationen je Lösung im Mittel, dichte Rückfälle: {1}"),
    FR("cg : {0} itérations par résolution en moyenne, replis sur le dense : {1}"),
    ES("cg: {0} iteraciones por resolución de media, vueltas al denso: {1}"),
    PT("cg: {0} iterações por resolução em média, recursos ao denso: {1}"),
    IT("cg: {0} iterazioni per risoluzione in media, ripieghi sul denso: {1}"),
    NL("cg: gemiddeld {0} iteraties per oplossing, terugvallen op dicht: {1}"),
    RU("cg: в среднем {0} итераций на решение, переходов к плотному: {1}"),
    TR("cg: çözüm başına ortalama {0} yineleme, yoğuna dönüş: {1}"));

SS_MSG(ba_res_time,
    EN("time: preprocessing {0} s, solving {1} s, total {2} s"),
    JA("時間: 前処理 {0} 秒、求解 {1} 秒、合計 {2} 秒"),
    ZH_HANS("耗时：预处理 {0} 秒，求解 {1} 秒，合计 {2} 秒"),
    ZH_HANT("耗時：預處理 {0} 秒，求解 {1} 秒，合計 {2} 秒"),
    KO("시간: 전처리 {0}초, 풀이 {1}초, 합계 {2}초"),
    DE("Zeit: Vorverarbeitung {0} s, Lösen {1} s, gesamt {2} s"),
    FR("temps : prétraitement {0} s, résolution {1} s, total {2} s"),
    ES("tiempo: preprocesado {0} s, resolución {1} s, total {2} s"),
    PT("tempo: pré-processamento {0} s, resolução {1} s, total {2} s"),
    IT("tempo: pre-elaborazione {0} s, risoluzione {1} s, totale {2} s"),
    NL("tijd: voorbewerking {0} s, oplossen {1} s, totaal {2} s"),
    RU("время: предобработка {0} с, решение {1} с, всего {2} с"),
    TR("süre: ön işleme {0} sn, çözme {1} sn, toplam {2} sn"));

SS_MSG(ba_res_vram,
    EN("vram: {0} MB"),
    JA("VRAM: {0} MB"),
    ZH_HANS("显存：{0} MB"),
    ZH_HANT("顯示記憶體：{0} MB"),
    KO("VRAM: {0} MB"),
    DE("VRAM: {0} MB"),
    FR("VRAM : {0} Mo"),
    ES("VRAM: {0} MB"),
    PT("VRAM: {0} MB"),
    IT("VRAM: {0} MB"),
    NL("VRAM: {0} MB"),
    RU("видеопамять: {0} МБ"),
    TR("VRAM: {0} MB"));


SS_MSG(opt_sequence,
    EN("Use for images under these prefixes taken in filename order.\n- Matches neighbors and prioritizes them during mapping to avoid repeated-structure folds.\n- Same filenames across members share a position, e.g. cam0,cam1 for a rig.\n- Repeat for several sequences. Use `.` for the image directory."),
    JA("これらのパス接頭辞の下の画像はファイル名順に撮影されたものです（動画のフレーム、歩きながら撮ったフォルダー）。\n- 隣接画像同士をマッチングし、マッパーは隣接画像との対応を他のどれよりも先に信頼するので、繰り返し構造でモデルが自身の上に折り畳まれるのを防ぎます。\n- 2 つのメンバーの下で同名の画像は同じ位置を共有するため、リグのレンズは 1 つのシーケンスです（cam0,cam1）。\n- 複数のシーケンスには繰り返し指定します。 `.` は画像ディレクトリ自体です。"),
    ZH_HANS("这些路径前缀下的图像按文件名顺序拍摄（视频帧、边走边拍的文件夹）。相邻图像会被匹配，建图时先信任相邻图像的对应关系，再参考其他部分，以免重复结构把模型折叠到自身上。\n- 两个成员下同名的图像共享同一位置，所以一个装置的镜头是同一个序列（cam0,cam1）。\n- 多个序列可重复指定； `.` 表示图像目录本身。"),
    ZH_HANT("這些路徑前綴下的影像按檔名順序拍攝（影片影格、邊走邊拍的資料夾）。相鄰影像會被比對，建圖時先信任相鄰影像的對應關係，再參考其他部分，以免重複結構把模型摺疊到自身上。\n- 兩個成員下同名的影像共用同一位置，所以一個裝置的鏡頭是同一個序列（cam0,cam1）。\n- 多個序列可重複指定； `.` 表示影像目錄本身。"),
    KO("이 경로 접두사 아래의 이미지는 파일 이름 순서로 촬영되었습니다(동영상 프레임, 걸으며 찍은 폴더). 이웃끼리 매칭하고, 매퍼는 이웃과의 대응을 다른 무엇보다 먼저 신뢰하므로 반복되는 구조가 모델을 자기 위로 접는 일을 막습니다.\n- 두 멤버 아래 같은 이름의 이미지는 같은 위치를 공유하므로 리그의 렌즈들은 하나의 시퀀스입니다(cam0,cam1).\n- 여러 시퀀스에는 반복해서 지정합니다. `.`은 이미지 디렉터리 자체입니다."),
    DE("Die Bilder unter diesen Pfadpräfixen wurden in Dateinamensreihenfolge aufgenommen (die Frames eines Videos, ein im Gehen aufgenommener Ordner). Ihre Nachbarn werden zugeordnet, und der Mapper vertraut den Korrespondenzen eines Nachbarn vor allen anderen, was verhindert, dass wiederholte Struktur das Modell auf sich selbst faltet.\n- Bilder gleichen Namens unter zwei Mitgliedern teilen eine Position, sodass die Objektive eines Rigs eine Sequenz sind (cam0,cam1). Für mehrere Sequenzen wiederholen;\n- `.` ist das Bildverzeichnis selbst."),
    FR("Les images sous ces préfixes de chemin ont été prises dans l'ordre des noms de fichier (les images d'une vidéo, un dossier filmé en marchant). Leurs voisines sont appariées, et le mapper fait confiance aux correspondances d'une voisine avant toute autre, ce qui empêche une structure répétée de replier le modèle sur lui-même.\nLes images de même nom sous deux membres partagent une position, si bien que les objectifs d'un rig forment une seule séquence (cam0,cam1). À répéter pour plusieurs séquences ; `.` désigne le répertoire d'images lui-même."),
    ES("Las imágenes bajo estos prefijos de ruta se tomaron en orden de nombre de archivo (los fotogramas de un vídeo, una carpeta grabada caminando). Sus vecinas se emparejan, y el mapeador confía en las correspondencias de una vecina antes que en cualquier otra, lo que evita que una estructura repetida pliegue el modelo sobre sí mismo.\nLas imágenes con el mismo nombre bajo dos miembros comparten posición, así que las lentes de un rig son una sola secuencia (cam0,cam1). Repítase para varias secuencias; `.` es el propio directorio de imágenes."),
    PT("As imagens sob estes prefixos de caminho foram captadas por ordem de nome de ficheiro (os quadros de um vídeo, uma pasta gravada a andar). As suas vizinhas são comparadas, e o mapeador confia nas correspondências de uma vizinha antes de qualquer outra, o que impede que uma estrutura repetida dobre o modelo sobre si mesmo.\n- Imagens com o mesmo nome sob dois membros partilham uma posição, pelo que as lentes de um rig são uma só sequência (cam0,cam1). Repita para várias sequências;\n- `.` é o próprio diretório de imagens."),
    IT("Le immagini sotto questi prefissi di percorso sono state riprese in ordine di nome di file (i fotogrammi di un video, una cartella ripresa camminando).\nLe vicine vengono abbinate e il mapper si fida delle corrispondenze di una vicina prima di ogni altra, il che impedisce a una struttura ripetuta di ripiegare il modello su se stesso. Le immagini con lo stesso nome sotto due membri condividono una posizione, così gli obiettivi di un rig sono una sola sequenza (cam0,cam1). Ripetere per più sequenze; `.` è la directory delle immagini stessa."),
    NL("De beelden onder deze padvoorvoegsels zijn in bestandsnaamvolgorde opgenomen (de frames van een video, een al lopend opgenomen map).\n- Hun buren worden gematcht, en de mapper vertrouwt de correspondenties van een buur vóór alle andere, wat voorkomt dat herhaalde structuur het model op zichzelf vouwt. Beelden met dezelfde naam onder twee leden delen een positie, zodat de lenzen van een rig één reeks zijn (cam0,cam1).\n- Herhaal voor meerdere reeksen; `.` is de beeldmap zelf."),
    RU("Изображения под этими префиксами пути сняты в порядке имён файлов (кадры видео, папка, снятая на ходу).\n- Их соседи сопоставляются, а маппер доверяет соответствиям соседа прежде всех остальных, что не даёт повторяющейся структуре сложить модель саму на себя.\n- Изображения с одинаковым именем под двумя элементами делят одну позицию, поэтому объективы рига — одна последовательность (cam0,cam1). Повторите для нескольких последовательностей; `.` — сам каталог изображений."),
    TR("Bu yol öneklerinin altındaki görüntüler dosya adı sırasıyla çekilmiştir (bir videonun kareleri, yürürken çekilen bir klasör). Komşuları eşleştirilir ve haritalayıcı bir komşunun eşleşmelerine diğer her şeyden önce güvenir; bu, yinelenen yapının modeli kendi üzerine katlamasını önler.\n- İki üyenin altındaki aynı adlı görüntüler aynı konumu paylaşır, bu yüzden bir rigin lensleri tek bir dizidir (cam0,cam1). Birden çok dizi için yineleyin;\n- `.` görüntü dizininin kendisidir."));

}  // namespace sfmhelp
}  // namespace msg
}  // namespace i18n
}  // namespace spirula

#include "i18n/EndCatalog.h"
