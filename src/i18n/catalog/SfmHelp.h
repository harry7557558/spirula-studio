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
    EN("bundle-adjust a BAL problem (solver benchmark)"),
    JA("BAL 形式の問題をバンドル調整します（ソルバーのベンチマーク）"),
    ZH_HANS("对 BAL 格式的问题做平差（求解器基准测试）"),
    ZH_HANT("對 BAL 格式的問題做平差（求解器基準測試）"),
    KO("BAL 형식 문제를 번들 조정합니다(솔버 벤치마크)"),
    DE("ein BAL-Problem ausgleichen (Löser-Benchmark)"),
    FR("ajuster un problème BAL (banc d'essai du solveur)"),
    ES("ajustar un problema BAL (banco de pruebas del solucionador)"),
    PT("ajustar um problema BAL (banco de provas do solucionador)"),
    IT("aggiustare un problema BAL (banco di prova del risolutore)"),
    NL("een BAL-probleem aanpassen (oplosser-benchmark)"),
    RU("уравнять задачу в формате BAL (тест решателя)"),
    TR("bir BAL problemini dengele (çözücü kıyaslaması)"));

// ===========================================================================
// What each command does, at length
// ===========================================================================

SS_MSG(desc_auto_1,
    EN("Runs extract -> match -> map -> merge with COLMAP's "
       "automatic_reconstructor presets and writes "
       "WORKSPACE/{features,matches.bin,sparse/0}. A capture that is not one "
       "connected view graph also writes sparse/1, sparse/2, ... -- the "
       "remaining components, largest first."),
    JA("extract -> match -> map -> merge を COLMAP の automatic_reconstructor "
       "プリセットで走らせ、WORKSPACE/{features,matches.bin,sparse/0} を書き出します。"
       "1 つの連結した視点グラフになっていない撮影では sparse/1、sparse/2、... も"
       "書き出します（残りの成分を大きい順に）。"),
    ZH_HANS("按 COLMAP 的 automatic_reconstructor 预设依次运行 extract -> match -> "
            "map -> merge，并写出 WORKSPACE/{features,matches.bin,sparse/0}。"
            "若拍摄不是一个连通的视图图，还会写出 sparse/1、sparse/2……即其余的连通块，"
            "从大到小。"),
    ZH_HANT("按 COLMAP 的 automatic_reconstructor 預設依次執行 extract -> match -> "
            "map -> merge，並寫出 WORKSPACE/{features,matches.bin,sparse/0}。"
            "若拍攝不是一個連通的視圖圖，還會寫出 sparse/1、sparse/2……即其餘的連通塊，"
            "從大到小。"),
    KO("extract -> match -> map -> merge 를 COLMAP 의 automatic_reconstructor "
       "프리셋으로 돌리고 WORKSPACE/{features,matches.bin,sparse/0} 을 씁니다. "
       "하나로 이어진 뷰 그래프가 아닌 촬영이면 sparse/1, sparse/2, ... 도 씁니다 "
       "-- 남은 성분을 큰 것부터."),
    DE("Führt extract -> match -> map -> merge mit den Voreinstellungen von "
       "COLMAPs automatic_reconstructor aus und schreibt "
       "WORKSPACE/{features,matches.bin,sparse/0}. Eine Aufnahme, die kein "
       "zusammenhängender Sichtgraph ist, schreibt zusätzlich sparse/1, "
       "sparse/2, ... -- die übrigen Komponenten, die größte zuerst."),
    FR("Enchaîne extract -> match -> map -> merge avec les préréglages de "
       "l'automatic_reconstructor de COLMAP et écrit "
       "WORKSPACE/{features,matches.bin,sparse/0}. Une prise de vue qui n'est "
       "pas un seul graphe de vues connexe écrit aussi sparse/1, sparse/2, ... "
       "-- les composantes restantes, la plus grande d'abord."),
    ES("Ejecuta extract -> match -> map -> merge con los ajustes del "
       "automatic_reconstructor de COLMAP y escribe "
       "WORKSPACE/{features,matches.bin,sparse/0}. Una captura que no es un "
       "único grafo de vistas conexo escribe además sparse/1, sparse/2, ...: las "
       "componentes restantes, la mayor primero."),
    PT("Executa extract -> match -> map -> merge com as predefinições do "
       "automatic_reconstructor do COLMAP e escreve "
       "WORKSPACE/{features,matches.bin,sparse/0}. Uma captura que não é um único "
       "grafo de vistas conexo escreve também sparse/1, sparse/2, ...: os "
       "componentes restantes, o maior primeiro."),
    IT("Esegue extract -> match -> map -> merge con le preimpostazioni "
       "dell'automatic_reconstructor di COLMAP e scrive "
       "WORKSPACE/{features,matches.bin,sparse/0}. Una ripresa che non è un "
       "unico grafo delle viste connesso scrive anche sparse/1, sparse/2, ...: "
       "le componenti restanti, la più grande per prima."),
    NL("Draait extract -> match -> map -> merge met de voorinstellingen van "
       "COLMAP's automatic_reconstructor en schrijft "
       "WORKSPACE/{features,matches.bin,sparse/0}. Een opname die geen "
       "samenhangende zichtgraaf is, schrijft ook sparse/1, sparse/2, ... -- de "
       "overige componenten, de grootste eerst."),
    RU("Выполняет extract -> match -> map -> merge с предустановками "
       "automatic_reconstructor из COLMAP и пишет "
       "WORKSPACE/{features,matches.bin,sparse/0}. Съёмка, не образующая один "
       "связный граф видов, даёт ещё sparse/1, sparse/2, ... -- остальные "
       "компоненты, начиная с наибольшей."),
    TR("extract -> match -> map -> merge adımlarını COLMAP'in "
       "automatic_reconstructor ön ayarlarıyla çalıştırır ve "
       "WORKSPACE/{features,matches.bin,sparse/0} yazar. Tek bir bağlı görüş "
       "çizgesi olmayan bir çekim ayrıca sparse/1, sparse/2, ... yazar -- kalan "
       "bileşenler, en büyükten başlayarak."));

SS_MSG(desc_auto_2,
    EN("The positional defaults to `images`; a dataset directory containing "
       "`images/` is accepted and that sub-directory used, so `auto DATASET` and "
       "`auto DATASET/images` behave the same. `masks` beside the image "
       "directory is picked up automatically."),
    JA("位置引数の既定は `images` です。`images/` を含むデータセットディレクトリも"
       "受け付け、そのサブディレクトリを使うため、`auto DATASET` と "
       "`auto DATASET/images` は同じ動きになります。画像ディレクトリの隣にある "
       "`masks` は自動的に拾われます。"),
    ZH_HANS("位置参数默认为 `images`；也接受包含 `images/` 的数据集目录并改用该子目录，"
            "因此 `auto DATASET` 与 `auto DATASET/images` 行为相同。"
            "图像目录旁边的 `masks` 会被自动采用。"),
    ZH_HANT("位置參數預設為 `images`；也接受包含 `images/` 的資料集目錄並改用該子目錄，"
            "因此 `auto DATASET` 與 `auto DATASET/images` 行為相同。"
            "影像目錄旁邊的 `masks` 會被自動採用。"),
    KO("위치 인자의 기본값은 `images` 입니다. `images/` 를 담은 데이터셋 디렉터리도 "
       "받아 그 하위 디렉터리를 쓰므로 `auto DATASET` 과 `auto DATASET/images` 는 "
       "같게 동작합니다. 이미지 디렉터리 옆의 `masks` 는 저절로 집어 옵니다."),
    DE("Das Positionsargument ist standardmäßig `images`; ein "
       "Datensatzverzeichnis mit `images/` wird ebenfalls angenommen und jenes "
       "Unterverzeichnis benutzt, sodass `auto DATASET` und `auto "
       "DATASET/images` gleich wirken. Ein `masks` neben dem Bildverzeichnis "
       "wird von selbst aufgegriffen."),
    FR("Le positionnel vaut `images` par défaut ; un dossier de jeu de données "
       "contenant `images/` est accepté et ce sous-dossier employé, si bien que "
       "`auto DATASET` et `auto DATASET/images` se comportent pareil. Un `masks` "
       "à côté du dossier d'images est repris automatiquement."),
    ES("El posicional vale `images` por defecto; también se acepta una carpeta "
       "de conjunto de datos que contenga `images/` y se usa esa subcarpeta, así "
       "que `auto DATASET` y `auto DATASET/images` se comportan igual. Un "
       "`masks` junto a la carpeta de imágenes se recoge solo."),
    PT("O posicional vale `images` por padrão; também se aceita uma pasta de "
       "conjunto de dados que contenha `images/` e usa-se essa subpasta, de modo "
       "que `auto DATASET` e `auto DATASET/images` se comportam igual. Um "
       "`masks` ao lado da pasta de imagens é apanhado sozinho."),
    IT("Il posizionale vale `images` per impostazione predefinita; si accetta "
       "anche una cartella di dataset che contenga `images/` e si usa quella "
       "sottocartella, così `auto DATASET` e `auto DATASET/images` si comportano "
       "allo stesso modo. Un `masks` accanto alla cartella delle immagini viene "
       "raccolto da sé."),
    NL("Het positionele argument is standaard `images`; een datasetmap met "
       "`images/` erin wordt ook aanvaard en die submap gebruikt, zodat `auto "
       "DATASET` en `auto DATASET/images` hetzelfde doen. Een `masks` naast de "
       "beeldmap wordt vanzelf opgepakt."),
    RU("Позиционный аргумент по умолчанию -- `images`; принимается и каталог "
       "набора данных, содержащий `images/`, и берётся этот подкаталог, так что "
       "`auto DATASET` и `auto DATASET/images` ведут себя одинаково. `masks` "
       "рядом с каталогом изображений подхватывается сам."),
    TR("Konumsal argüman varsayılan olarak `images`'tır; `images/` içeren bir "
       "veri kümesi dizini de kabul edilir ve o alt dizin kullanılır, böylece "
       "`auto DATASET` ile `auto DATASET/images` aynı davranır. Görüntü dizininin "
       "yanındaki `masks` kendiliğinden alınır."));

SS_MSG(desc_auto_3,
    EN("Two knobs decide the rest: --quality and --data-type. Anything they set "
       "can be overridden by naming the flag explicitly, and the run reports "
       "what they moved."),
    JA("残りは --quality と --data-type の 2 つのつまみが決めます。これらが設定した"
       "ものはフラグを明示すれば上書きでき、実行時にはそれらが動かした内容が"
       "報告されます。"),
    ZH_HANS("其余由两个旋钮决定：--quality 与 --data-type。它们设定的任何值都可以"
            "通过显式写出对应标志来覆盖，运行时也会报告它们改动了什么。"),
    ZH_HANT("其餘由兩個旋鈕決定：--quality 與 --data-type。它們設定的任何值都可以"
            "透過明確寫出對應旗標來覆蓋，執行時也會回報它們改動了什麼。"),
    KO("나머지는 --quality 와 --data-type 두 손잡이가 정합니다. 이들이 정한 값은 "
       "해당 플래그를 직접 쓰면 덮어쓸 수 있고, 실행 중에 무엇을 옮겼는지 알려 줍니다."),
    DE("Den Rest bestimmen zwei Regler: --quality und --data-type. Was sie "
       "setzen, lässt sich durch ausdrückliches Nennen der Option überschreiben, "
       "und der Lauf meldet, was sie bewegt haben."),
    FR("Deux molettes décident du reste : --quality et --data-type. Tout ce "
       "qu'elles fixent peut être remplacé en nommant l'option explicitement, et "
       "l'exécution indique ce qu'elles ont déplacé."),
    ES("Dos mandos deciden el resto: --quality y --data-type. Todo lo que fijan "
       "puede sustituirse nombrando la opción de forma explícita, y la ejecución "
       "informa de lo que movieron."),
    PT("Dois botões decidem o resto: --quality e --data-type. Tudo o que eles "
       "fixam pode ser substituído nomeando a opção explicitamente, e a execução "
       "relata o que eles moveram."),
    IT("Due manopole decidono il resto: --quality e --data-type. Tutto ciò che "
       "fissano si può sovrascrivere nominando l'opzione esplicitamente, e "
       "l'esecuzione riferisce che cosa hanno spostato."),
    NL("Twee knoppen bepalen de rest: --quality en --data-type. Alles wat zij "
       "instellen kan overschreven worden door de optie uitdrukkelijk te noemen, "
       "en de run meldt wat zij verschoven hebben."),
    RU("Остальное решают две ручки: --quality и --data-type. Всё, что они "
       "задают, можно перекрыть, назвав флаг явно, а запуск сообщает, что именно "
       "они сдвинули."),
    TR("Gerisini iki düğme belirler: --quality ve --data-type. Bunların "
       "belirlediği her şey, ilgili bayrağı açıkça yazarak değiştirilebilir ve "
       "çalışma neyi oynattıklarını bildirir."));

SS_MSG(desc_extract_1,
    EN("GPU SIFT over one image or, recursively, a directory. A directory reuses "
       "one GPU context and processes largest-first, so device buffers are "
       "allocated once; decoding runs on a pool sized to fit --decode-budget, "
       "and peak memory does not grow with the number of images. Feature files "
       "mirror the image tree."),
    JA("画像 1 枚、またはディレクトリを再帰的にたどって GPU 上で SIFT を実行します。"
       "ディレクトリの場合は GPU コンテキストを 1 つ使い回し、大きいものから処理する"
       "ため、デバイス側のバッファは一度だけ確保されます。デコードは "
       "--decode-budget に収まる大きさのプールで動き、ピークメモリは画像枚数とともに"
       "増えません。特徴点ファイルは画像のツリー構造をそのまま写します。"),
    ZH_HANS("在 GPU 上对单张图像执行 SIFT，或递归遍历一个目录。处理目录时复用同一个 "
            "GPU 上下文并从大到小处理，因此设备缓冲只分配一次；解码在一个按 "
            "--decode-budget 定大小的线程池上进行，峰值内存不随图像数量增长。"
            "特征点文件与图像目录结构一一对应。"),
    ZH_HANT("在 GPU 上對單張影像執行 SIFT，或遞迴走訪一個目錄。處理目錄時重用同一個 "
            "GPU 內容並從大到小處理，因此裝置緩衝只配置一次；解碼在一個按 "
            "--decode-budget 定大小的執行緒池上進行，尖峰記憶體不隨影像數量增長。"
            "特徵點檔案與影像目錄結構一一對應。"),
    KO("이미지 하나 또는 디렉터리를 재귀로 돌며 GPU 에서 SIFT 를 실행합니다. "
       "디렉터리를 처리할 때는 GPU 컨텍스트 하나를 재사용하고 큰 것부터 처리하므로 "
       "장치 버퍼는 한 번만 잡습니다. 디코딩은 --decode-budget 에 맞춘 풀에서 돌고, "
       "최대 메모리는 이미지 수에 따라 늘지 않습니다. 특징점 파일은 이미지 트리를 "
       "그대로 본뜹니다."),
    DE("GPU-SIFT über ein Bild oder, rekursiv, über ein Verzeichnis. Ein "
       "Verzeichnis nutzt einen GPU-Kontext wieder und arbeitet die größten "
       "zuerst ab, sodass Gerätepuffer nur einmal belegt werden; das Dekodieren "
       "läuft auf einem Pool in der Größe von --decode-budget, und der "
       "Spitzenspeicher wächst nicht mit der Bildzahl. Die Merkmalsdateien "
       "spiegeln den Bildbaum."),
    FR("SIFT sur GPU pour une image ou, récursivement, pour un dossier. Un "
       "dossier réutilise un seul contexte GPU et traite les plus grandes "
       "d'abord, si bien que les tampons du périphérique ne sont alloués qu'une "
       "fois ; le décodage tourne sur une réserve taillée pour --decode-budget, "
       "et la mémoire de pointe ne croît pas avec le nombre d'images. Les "
       "fichiers de points reproduisent l'arborescence des images."),
    ES("SIFT en la GPU sobre una imagen o, de forma recursiva, sobre una "
       "carpeta. Una carpeta reutiliza un solo contexto de GPU y procesa las "
       "mayores primero, así que los búferes del dispositivo se reservan una "
       "vez; la descodificación corre en una reserva ajustada a "
       "--decode-budget, y la memoria de pico no crece con el número de "
       "imágenes. Los archivos de rasgos reflejan el árbol de imágenes."),
    PT("SIFT na GPU sobre uma imagem ou, recursivamente, sobre uma pasta. Uma "
       "pasta reaproveita um único contexto de GPU e processa as maiores "
       "primeiro, então os buffers do dispositivo são reservados uma vez; a "
       "decodificação corre numa reserva ajustada a --decode-budget, e a memória "
       "de pico não cresce com o número de imagens. Os arquivos de traços "
       "espelham a árvore de imagens."),
    IT("SIFT su GPU per un'immagine o, ricorsivamente, per una cartella. Una "
       "cartella riusa un solo contesto GPU e lavora prima le più grandi, così i "
       "buffer del dispositivo si allocano una volta sola; la decodifica gira su "
       "un pool dimensionato su --decode-budget, e la memoria di picco non "
       "cresce col numero di immagini. I file dei punti rispecchiano l'albero "
       "delle immagini."),
    NL("GPU-SIFT over één beeld of, recursief, over een map. Een map hergebruikt "
       "één GPU-context en verwerkt de grootste eerst, zodat apparaatbuffers "
       "eenmaal worden gereserveerd; het decoderen draait op een pool ter grootte "
       "van --decode-budget, en het piekgeheugen groeit niet mee met het aantal "
       "beelden. De kenmerkbestanden spiegelen de beeldboom."),
    RU("SIFT на GPU по одному изображению или, рекурсивно, по каталогу. Для "
       "каталога переиспользуется один контекст GPU, и обработка идёт от "
       "крупнейших, так что буферы устройства выделяются однажды; декодирование "
       "работает в пуле под --decode-budget, а пиковая память не растёт с числом "
       "изображений. Файлы признаков повторяют дерево изображений."),
    TR("Bir görüntü üzerinde ya da özyinelemeli olarak bir dizin üzerinde GPU "
       "SIFT. Bir dizin tek bir GPU bağlamını yeniden kullanır ve en büyükten "
       "başlayarak işler, böylece aygıt tamponları bir kez ayrılır; çözme "
       "--decode-budget'a göre boyutlanmış bir havuzda çalışır ve tepe bellek "
       "görüntü sayısıyla büyümez. Öznitelik dosyaları görüntü ağacını yansıtır."));

SS_MSG(desc_extract_2,
    EN("Keypoints are written in the *source* image's coordinates even when "
       "--max-image-size downscaled it, and each file records what EXIF said "
       "about the focal length and the camera, so `match` and `map` can build "
       "intrinsics for the images on disk without seeing them (D46)."),
    JA("--max-image-size で縮小した場合でも、キーポイントは*元*画像の座標で"
       "書き出されます。また各ファイルには EXIF が伝える焦点距離とカメラの情報が"
       "記録されるため、`match` と `map` はディスク上の画像を見ずに内部パラメータを"
       "組み立てられます（D46）。"),
    ZH_HANS("即使 --max-image-size 缩小过图像，关键点仍按*源*图像的坐标写出；"
            "每个文件还记录 EXIF 关于焦距与相机的信息，因此 `match` 与 `map` "
            "无需看到磁盘上的图像也能构造内参（D46）。"),
    ZH_HANT("即使 --max-image-size 縮小過影像，關鍵點仍按*原始*影像的座標寫出；"
            "每個檔案還記錄 EXIF 關於焦距與相機的資訊，因此 `match` 與 `map` "
            "無需看到磁碟上的影像也能建構內參（D46）。"),
    KO("--max-image-size 로 줄였더라도 키포인트는 *원본* 이미지 좌표로 씁니다. 또 "
       "각 파일에 EXIF 가 말한 초점 거리와 카메라 정보를 적어 두므로, `match` 와 "
       "`map` 은 디스크의 이미지를 보지 않고도 내부 파라미터를 세울 수 있습니다"
       "(D46)."),
    DE("Schlüsselpunkte werden in den Koordinaten des *Quellbildes* geschrieben, "
       "selbst wenn --max-image-size es verkleinert hat, und jede Datei hält "
       "fest, was EXIF über Brennweite und Kamera sagte, sodass `match` und "
       "`map` Intrinsics für die Bilder auf der Platte bauen können, ohne sie zu "
       "sehen (D46)."),
    FR("Les points clés sont écrits dans les coordonnées de l'image *source*, "
       "même quand --max-image-size l'a réduite, et chaque fichier consigne ce "
       "que l'EXIF disait de la focale et de l'appareil, de sorte que `match` et "
       "`map` peuvent bâtir les paramètres internes des images sur disque sans "
       "les voir (D46)."),
    ES("Los puntos clave se escriben en coordenadas de la imagen *de origen*, "
       "incluso cuando --max-image-size la redujo, y cada archivo anota lo que "
       "el EXIF decía de la focal y de la cámara, de modo que `match` y `map` "
       "pueden construir los parámetros internos de las imágenes en disco sin "
       "verlas (D46)."),
    PT("Os pontos-chave são escritos nas coordenadas da imagem *de origem*, "
       "mesmo quando --max-image-size a reduziu, e cada arquivo anota o que o "
       "EXIF dizia da focal e da câmera, de modo que `match` e `map` podem "
       "construir os parâmetros internos das imagens em disco sem as ver (D46)."),
    IT("I punti chiave sono scritti nelle coordinate dell'immagine *di origine*, "
       "anche quando --max-image-size l'ha ridotta, e ogni file annota ciò che "
       "l'EXIF diceva della focale e della camera, così `match` e `map` possono "
       "costruire i parametri interni delle immagini su disco senza vederle "
       "(D46)."),
    NL("Sleutelpunten worden geschreven in de coördinaten van het *bronbeeld*, "
       "ook wanneer --max-image-size het verkleind heeft, en elk bestand noteert "
       "wat de EXIF zei over brandpuntsafstand en camera, zodat `match` en `map` "
       "intrinsieken voor de beelden op schijf kunnen bouwen zonder ze te zien "
       "(D46)."),
    RU("Ключевые точки пишутся в координатах *исходного* изображения, даже когда "
       "--max-image-size его уменьшил, а каждый файл сохраняет то, что EXIF "
       "сообщил о фокусном расстоянии и камере, так что `match` и `map` строят "
       "внутренние параметры для изображений на диске, не видя их (D46)."),
    TR("Anahtar noktalar, --max-image-size küçültmüş olsa bile *kaynak* "
       "görüntünün koordinatlarıyla yazılır ve her dosya EXIF'in odak uzaklığı "
       "ile kamera hakkında söylediğini kaydeder; böylece `match` ve `map`, "
       "diskteki görüntüleri görmeden onlar için iç parametre kurabilir (D46)."));

SS_MSG(desc_match_1,
    EN("Brute-force descriptor matching on the GPU, then two-view geometric "
       "verification (F/H RANSAC) on a worker pool fed by the matcher. "
       "Verification is where the time goes; --threads sizes it and results do "
       "not depend on the count."),
    JA("GPU 上で総当たりの記述子マッチングを行い、続いてマッチャーが供給する"
       "ワーカープールで 2 視点の幾何検証（F/H の RANSAC）を行います。時間はこの検証で"
       "使われます。--threads がその規模を決めますが、結果はその数に依存しません。"),
    ZH_HANS("先在 GPU 上做暴力描述子匹配，再由匹配器供料的工作线程池做双视图几何验证"
            "（F/H 的 RANSAC）。时间主要花在验证上；--threads 决定其规模，"
            "而结果与线程数无关。"),
    ZH_HANT("先在 GPU 上做暴力描述子匹配，再由匹配器供料的工作執行緒池做雙視圖幾何驗證"
            "（F/H 的 RANSAC）。時間主要花在驗證上；--threads 決定其規模，"
            "而結果與執行緒數無關。"),
    KO("GPU 에서 전수 기술자 매칭을 한 뒤, 매처가 먹여 주는 작업 풀에서 두 시점 기하 "
       "검증(F/H RANSAC)을 합니다. 시간은 이 검증에서 쓰이며, --threads 가 그 규모를 "
       "정하지만 결과는 그 수에 좌우되지 않습니다."),
    DE("Deskriptorzuordnung mit Gewalt auf der GPU, danach geometrische "
       "Zweiansichtsprüfung (F/H-RANSAC) auf einem Arbeiterpool, den der Matcher "
       "speist. Die Zeit steckt in der Prüfung; --threads bemisst sie, und die "
       "Ergebnisse hängen nicht von der Zahl ab."),
    FR("Appariement de descripteurs en force brute sur le GPU, puis vérification "
       "géométrique à deux vues (RANSAC F/H) sur une réserve d'ouvriers "
       "alimentée par l'apparieur. C'est la vérification qui prend le temps ; "
       "--threads la dimensionne et les résultats ne dépendent pas du nombre."),
    ES("Emparejamiento de descriptores por fuerza bruta en la GPU y luego "
       "verificación geométrica de dos vistas (RANSAC de F/H) en una reserva de "
       "trabajadores que alimenta el emparejador. El tiempo se va en la "
       "verificación; --threads la dimensiona y los resultados no dependen del "
       "número."),
    PT("Emparelhamento de descritores por força bruta na GPU e depois "
       "verificação geométrica de duas vistas (RANSAC de F/H) numa reserva de "
       "trabalhadores alimentada pelo emparelhador. O tempo vai-se na "
       "verificação; --threads dimensiona-a e os resultados não dependem do "
       "número."),
    IT("Abbinamento dei descrittori a forza bruta sulla GPU, poi verifica "
       "geometrica a due viste (RANSAC F/H) su un pool di worker alimentato "
       "dall'abbinatore. Il tempo se ne va nella verifica; --threads la "
       "dimensiona e i risultati non dipendono dal numero."),
    NL("Brute-force descriptormatching op de GPU, daarna meetkundige "
       "tweebeeldverificatie (F/H-RANSAC) op een werkerspool die de matcher "
       "voedt. De tijd gaat in de verificatie zitten; --threads bepaalt de "
       "omvang en de uitkomsten hangen niet van het aantal af."),
    RU("Полный перебор при сопоставлении дескрипторов на GPU, затем двухвидовая "
       "геометрическая проверка (RANSAC для F/H) в пуле рабочих потоков, который "
       "питает сопоставитель. Время уходит на проверку; --threads задаёт её "
       "размер, а результаты от их числа не зависят."),
    TR("GPU'da kaba kuvvetle betimleyici eşleştirme, ardından eşleştiricinin "
       "beslediği bir işçi havuzunda iki görüşlü geometrik doğrulama (F/H "
       "RANSAC). Zaman doğrulamada geçer; --threads onu boyutlandırır ve "
       "sonuçlar sayıya bağlı değildir."));

SS_MSG(desc_match_2,
    EN("A fisheye --camera-model switches verification to unit bearings, where "
       "the epipolar constraint holds at any field of view; the focal is "
       "searched per camera group on a sample of pairs unless --focal or EXIF "
       "gives one (D45/D46). The camera grouping and the focals this stage "
       "settles on travel in the match database, so `map` inherits them instead "
       "of re-deriving them (D47)."),
    JA("魚眼の --camera-model を指定すると、検証は単位方位ベクトルに切り替わります。"
       "そこではエピポーラ拘束がどんな画角でも成り立ちます。--focal や EXIF が"
       "焦点距離を与えない場合は、ペアの標本上でカメラグループごとに探索します"
       "（D45/D46）。この段階が定めたカメラのグループ分けと焦点距離は対応データベースに"
       "入るので、`map` は導き直さずに引き継ぎます（D47）。"),
    ZH_HANS("指定鱼眼的 --camera-model 后，验证会改用单位方向向量，此时对极约束在"
            "任意视场角下都成立；若 --focal 或 EXIF 没有给出焦距，则在一部分像对上"
            "按相机组搜索（D45/D46）。本阶段定下的相机分组与焦距会写入匹配数据库，"
            "因此 `map` 直接沿用而不再重新推导（D47）。"),
    ZH_HANT("指定魚眼的 --camera-model 後，驗證會改用單位方向向量，此時對極約束在"
            "任意視場角下都成立；若 --focal 或 EXIF 沒有給出焦距，則在一部分影像對上"
            "按相機群組搜尋（D45/D46）。本階段定下的相機分組與焦距會寫入匹配資料庫，"
            "因此 `map` 直接沿用而不再重新推導（D47）。"),
    KO("어안 --camera-model 을 주면 검증이 단위 방향 벡터로 바뀌며, 그곳에서는 "
       "에피폴라 제약이 어떤 화각에서도 성립합니다. --focal 이나 EXIF 가 초점 거리를 "
       "주지 않으면 쌍의 표본에서 카메라 그룹별로 탐색합니다(D45/D46). 이 단계가 "
       "정한 카메라 묶음과 초점 거리는 대응 데이터베이스에 실려 가므로, `map` 은 "
       "다시 끌어내지 않고 물려받습니다(D47)."),
    DE("Ein Fischaugen---camera-model schaltet die Prüfung auf "
       "Einheitsrichtungen um, wo die Epipolarbedingung bei jedem Bildwinkel "
       "gilt; die Brennweite wird je Kameragruppe an einer Stichprobe von Paaren "
       "gesucht, sofern nicht --focal oder EXIF eine liefert (D45/D46). Die "
       "Kameragruppierung und die Brennweiten, auf die sich diese Stufe "
       "festlegt, reisen in der Zuordnungsdatenbank mit, sodass `map` sie erbt, "
       "statt sie neu herzuleiten (D47)."),
    FR("Un --camera-model fisheye fait passer la vérification à des directions "
       "unitaires, où la contrainte épipolaire tient à n'importe quel champ ; la "
       "focale est cherchée par groupe de caméras sur un échantillon de paires, "
       "sauf si --focal ou l'EXIF en fournit une (D45/D46). Le regroupement des "
       "caméras et les focales arrêtés à cette étape voyagent dans la base "
       "d'appariements, si bien que `map` en hérite au lieu de les redériver "
       "(D47)."),
    ES("Un --camera-model de ojo de pez pasa la verificación a direcciones "
       "unitarias, donde la restricción epipolar se cumple con cualquier campo "
       "de visión; la focal se busca por grupo de cámaras sobre una muestra de "
       "pares, salvo que --focal o el EXIF den una (D45/D46). La agrupación de "
       "cámaras y las focales que fija esta etapa viajan en la base de "
       "emparejamientos, así que `map` las hereda en vez de rededucirlas (D47)."),
    PT("Um --camera-model olho de peixe passa a verificação a direções "
       "unitárias, onde a restrição epipolar vale em qualquer campo de visão; a "
       "focal é procurada por grupo de câmeras numa amostra de pares, salvo se "
       "--focal ou o EXIF derem uma (D45/D46). O agrupamento de câmeras e as "
       "focais que esta etapa fixa viajam na base de correspondências, então "
       "`map` herda-os em vez de os deduzir de novo (D47)."),
    IT("Un --camera-model fisheye porta la verifica su direzioni unitarie, dove "
       "il vincolo epipolare vale a qualunque campo visivo; la focale si cerca "
       "per gruppo di camere su un campione di coppie, a meno che --focal o "
       "l'EXIF non ne diano una (D45/D46). Il raggruppamento delle camere e le "
       "focali fissate da questa fase viaggiano nel database degli abbinamenti, "
       "così `map` le eredita invece di riderivarle (D47)."),
    NL("Een fisheye---camera-model zet de verificatie om naar "
       "eenheidsrichtingen, waar de epipolaire voorwaarde bij elk beeldveld "
       "geldt; de brandpuntsafstand wordt per cameragroep gezocht op een "
       "steekproef van paren, tenzij --focal of de EXIF er een geeft (D45/D46). "
       "De cameragroepering en de brandpuntsafstanden die deze fase vastlegt "
       "reizen mee in de matchdatabase, zodat `map` ze erft in plaats van ze "
       "opnieuw af te leiden (D47)."),
    RU("Рыбий глаз в --camera-model переводит проверку на единичные направления, "
       "где эпиполярное условие выполняется при любом поле зрения; фокусное "
       "расстояние ищется по группам камер на выборке пар, если его не дают "
       "--focal или EXIF (D45/D46). Группировка камер и фокусные расстояния, на "
       "которых остановилась эта стадия, едут в базе соответствий, так что `map` "
       "наследует их, а не выводит заново (D47)."),
    TR("Balıkgözü bir --camera-model, doğrulamayı birim yön vektörlerine "
       "geçirir; orada epipolar kısıt her görüş açısında geçerlidir. --focal ya "
       "da EXIF bir odak vermiyorsa, odak bir çift örneklemi üzerinde kamera "
       "grubu başına aranır (D45/D46). Bu aşamanın karar kıldığı kamera "
       "gruplaması ve odaklar eşleşme veritabanıyla birlikte gider, böylece "
       "`map` onları yeniden türetmek yerine devralır (D47)."));

SS_MSG(desc_map_1,
    EN("Seed, register, triangulate, bundle-adjust, filter -- then the merge "
       "levels and the finishing passes. A capture that is not one connected "
       "view graph reconstructs as several models, written to <out>/0, <out>/1, "
       "... largest first (COLMAP's layout); they are then merged where they "
       "share images and grown into whatever else they can register."),
    JA("シード選び、登録、三角測量、バンドル調整、フィルタリング -- そのあとに統合の"
       "各段と仕上げのパスが続きます。1 つの連結した視点グラフになっていない撮影は"
       "複数のモデルとして再構成され、<out>/0、<out>/1、... に大きい順で書き出されます"
       "（COLMAP の配置）。それらは画像を共有するところで統合され、さらに登録できる"
       "ものを取り込んで成長します。"),
    ZH_HANS("选种子、注册、三角化、平差、过滤 —— 然后是各层合并与收尾阶段。"
            "若拍摄不是一个连通的视图图，就会重建成多个模型，按从大到小写入 "
            "<out>/0、<out>/1……（COLMAP 的布局）；随后在共享图像处把它们合并，"
            "并把还能注册的图像继续并入。"),
    ZH_HANT("選種子、註冊、三角化、平差、過濾 —— 然後是各層合併與收尾階段。"
            "若拍攝不是一個連通的視圖圖，就會重建成多個模型，按從大到小寫入 "
            "<out>/0、<out>/1……（COLMAP 的配置）；隨後在共享影像處把它們合併，"
            "並把還能註冊的影像繼續併入。"),
    KO("씨앗 고르기, 등록, 삼각측량, 번들 조정, 걸러내기 -- 그다음 병합 층들과 마무리 "
       "단계가 이어집니다. 하나로 이어진 뷰 그래프가 아닌 촬영은 여러 모델로 "
       "재구성되어 <out>/0, <out>/1, ... 에 큰 것부터 쓰입니다(COLMAP 의 배치). "
       "이후 이미지를 공유하는 곳에서 병합되고, 더 등록할 수 있는 것을 받아들이며 "
       "자랍니다."),
    DE("Startpaar, Registrieren, Triangulieren, Ausgleichen, Filtern -- dann die "
       "Zusammenführungsebenen und die Schlussdurchgänge. Eine Aufnahme, die "
       "kein zusammenhängender Sichtgraph ist, wird zu mehreren Modellen, "
       "geschrieben nach <out>/0, <out>/1, ... das größte zuerst (COLMAPs "
       "Anordnung); sie werden dort zusammengeführt, wo sie Bilder teilen, und "
       "um alles erweitert, was sie sonst noch registrieren können."),
    FR("Amorcer, enregistrer, trianguler, ajuster, filtrer -- puis les niveaux "
       "de fusion et les passes de finition. Une prise de vue qui n'est pas un "
       "seul graphe de vues connexe se reconstruit en plusieurs modèles, écrits "
       "dans <out>/0, <out>/1, ... du plus grand au plus petit (la disposition "
       "de COLMAP) ; ils sont ensuite fusionnés là où ils partagent des images, "
       "et étendus à tout ce qu'ils peuvent encore enregistrer."),
    ES("Sembrar, registrar, triangular, ajustar, filtrar; luego los niveles de "
       "fusión y las pasadas de acabado. Una captura que no es un único grafo de "
       "vistas conexo se reconstruye como varios modelos, escritos en <out>/0, "
       "<out>/1, ... del mayor al menor (la disposición de COLMAP); después se "
       "fusionan donde comparten imágenes y crecen con todo lo que aún puedan "
       "registrar."),
    PT("Semear, registrar, triangular, ajustar, filtrar -- depois os níveis de "
       "fusão e as passagens de acabamento. Uma captura que não é um único grafo "
       "de vistas conexo reconstrói-se como vários modelos, escritos em "
       "<out>/0, <out>/1, ... do maior para o menor (a disposição do COLMAP); "
       "em seguida são fundidos onde partilham imagens e crescem com tudo o que "
       "ainda conseguirem registrar."),
    IT("Innescare, registrare, triangolare, aggiustare, filtrare -- poi i "
       "livelli di fusione e i passaggi di rifinitura. Una ripresa che non è un "
       "unico grafo delle viste connesso si ricostruisce come più modelli, "
       "scritti in <out>/0, <out>/1, ... dal più grande (la disposizione di "
       "COLMAP); vengono poi fusi dove condividono immagini e accresciuti con "
       "tutto ciò che riescono ancora a registrare."),
    NL("Zaaien, registreren, trianguleren, aanpassen, filteren -- daarna de "
       "samenvoegniveaus en de afwerkgangen. Een opname die geen samenhangende "
       "zichtgraaf is, wordt gereconstrueerd als meerdere modellen, geschreven "
       "naar <out>/0, <out>/1, ... grootste eerst (COLMAP's indeling); daarna "
       "worden ze samengevoegd waar ze beelden delen en uitgebreid met al wat ze "
       "nog kunnen registreren."),
    RU("Начальная пара, регистрация, триангуляция, уравнивание, отсев -- затем "
       "уровни слияния и завершающие проходы. Съёмка, не образующая один связный "
       "граф видов, восстанавливается как несколько моделей, записываемых в "
       "<out>/0, <out>/1, ... начиная с наибольшей (раскладка COLMAP); потом их "
       "сливают там, где у них общие изображения, и наращивают всем, что они ещё "
       "могут зарегистрировать."),
    TR("Tohumla, kaydet, üçgenle, dengele, ele -- ardından birleştirme düzeyleri "
       "ve bitirme geçişleri. Tek bir bağlı görüş çizgesi olmayan bir çekim "
       "birden çok model olarak oluşturulur ve <out>/0, <out>/1, ... içine en "
       "büyükten başlayarak yazılır (COLMAP'in düzeni); sonra görüntü "
       "paylaştıkları yerde birleştirilir ve kaydedebildikleri her şeyle "
       "büyütülür."));

SS_MSG(desc_map_2,
    EN("--camera-model, --focal and --distortion take either a dataset-wide "
       "value or PREFIX=VALUE naming one camera group by image path, so a rig "
       "can mix models. Say nothing about cameras and the setup recorded by "
       "verification is used as it stands."),
    JA("--camera-model と --focal と --distortion は、データセット全体に効く値の"
       "ほか、画像パスでカメラグループを 1 つ指定する PREFIX=VALUE の形も取れる"
       "ため、リグごとにモデルを混在させられます。カメラについて何も指定しなければ、"
       "検証が記録した構成がそのまま使われます。"),
    ZH_HANS("--camera-model、--focal 与 --distortion 既可给出作用于整个数据集的值，"
            "也可写成 PREFIX=VALUE，按图像路径指定某一个相机组，因此同一套设备可以"
            "混用不同模型。若完全不提相机，就直接沿用验证阶段记录下来的配置。"),
    ZH_HANT("--camera-model、--focal 與 --distortion 既可給出作用於整個資料集的值，"
            "也可寫成 PREFIX=VALUE，按影像路徑指定某一個相機群組，因此同一套設備"
            "可以混用不同模型。若完全不提相機，就直接沿用驗證階段記錄下來的組態。"),
    KO("--camera-model 과 --focal 과 --distortion 은 데이터셋 전체에 걸리는 값은 "
       "물론, 이미지 경로로 카메라 그룹 하나를 지목하는 PREFIX=VALUE 형태도 받으므로 "
       "한 리그에서 여러 모델을 섞을 수 있습니다. 카메라에 대해 아무것도 말하지 "
       "않으면 검증이 기록한 구성을 그대로 씁니다."),
    DE("--camera-model, --focal und --distortion nehmen entweder einen Wert für "
       "den ganzen Datensatz oder PREFIX=VALUE, das über den Bildpfad eine "
       "Kameragruppe benennt, sodass ein Rig Modelle mischen kann. Sagt man "
       "nichts über Kameras, wird der von der Prüfung vermerkte Aufbau "
       "unverändert verwendet."),
    FR("--camera-model, --focal et --distortion acceptent soit une valeur pour "
       "tout le jeu de données, soit PREFIX=VALUE désignant un groupe de caméras "
       "par chemin d'image, si bien qu'un rig peut mêler des modèles. Ne rien "
       "dire des caméras et la configuration consignée par la vérification est "
       "reprise telle quelle."),
    ES("--camera-model, --focal y --distortion aceptan o bien un valor para todo "
       "el conjunto de datos, o bien PREFIX=VALUE que nombra un grupo de cámaras "
       "por ruta de imagen, de modo que un equipo puede mezclar modelos. Si no se "
       "dice nada sobre las cámaras, se usa tal cual la configuración que anotó "
       "la verificación."),
    PT("--camera-model, --focal e --distortion aceitam ou um valor para todo o "
       "conjunto de dados, ou PREFIX=VALUE que nomeia um grupo de câmeras pelo "
       "caminho da imagem, de modo que um equipamento pode misturar modelos. Se "
       "nada for dito sobre as câmeras, usa-se tal como está a configuração que a "
       "verificação anotou."),
    IT("--camera-model, --focal e --distortion accettano o un valore per l'intero "
       "dataset o PREFIX=VALUE che nomina un gruppo di camere tramite il percorso "
       "dell'immagine, così un rig può mescolare modelli. Se non si dice nulla "
       "sulle camere, si usa così com'è la configurazione annotata dalla "
       "verifica."),
    NL("--camera-model, --focal en --distortion nemen ofwel een waarde voor de "
       "hele dataset, ofwel PREFIX=VALUE dat via het beeldpad één cameragroep "
       "noemt, zodat een rig modellen kan mengen. Zeg niets over camera's en de "
       "door de verificatie genoteerde opzet wordt onveranderd gebruikt."),
    RU("--camera-model, --focal и --distortion принимают либо значение на весь "
       "набор данных, либо PREFIX=VALUE, называющее одну группу камер по пути "
       "изображения, так что установка может смешивать модели. Ничего не сказав о "
       "камерах, вы получите тот состав, что записала проверка, как есть."),
    TR("--camera-model, --focal ve --distortion, ya tüm veri kümesi için bir "
       "değer alır ya da görüntü yoluyla tek bir kamera grubunu adlandıran "
       "PREFIX=VALUE biçimini alır; böylece bir düzenek modelleri karıştırabilir. "
       "Kameralar hakkında hiçbir şey söylenmezse, doğrulamanın kaydettiği düzen "
       "olduğu gibi kullanılır."));

SS_MSG(desc_merge_1,
    EN("Merges models on the images they share: those give the similarity "
       "transform between the two gauges (D43). A directory holding 0/, 1/, ... "
       "is expanded to all of them. Models built from different features cannot "
       "be merged."),
    JA("モデルどうしを、共有している画像の上で統合します。その画像が 2 つのゲージの間の"
       "相似変換を与えます（D43）。0/、1/、... を含むディレクトリはそのすべてに"
       "展開されます。異なる特徴点から作られたモデルは統合できません。"),
    ZH_HANS("在模型共享的图像上把它们合并：这些图像给出两套基准之间的相似变换（D43）。"
            "若给的是含 0/、1/……的目录，会展开为其中全部模型。"
            "由不同特征点构建的模型无法合并。"),
    ZH_HANT("在模型共享的影像上把它們合併：這些影像給出兩套基準之間的相似變換（D43）。"
            "若給的是含 0/、1/……的目錄，會展開為其中全部模型。"
            "由不同特徵點建構的模型無法合併。"),
    KO("모델들을 서로 공유하는 이미지 위에서 병합합니다. 그 이미지들이 두 기준 사이의 "
       "닮음 변환을 줍니다(D43). 0/, 1/, ... 을 담은 디렉터리는 그 전부로 펼쳐집니다. "
       "서로 다른 특징점으로 만든 모델은 병합할 수 없습니다."),
    DE("Führt Modelle über die Bilder zusammen, die sie teilen: diese liefern die "
       "Ähnlichkeitstransformation zwischen den beiden Eichungen (D43). Ein "
       "Verzeichnis mit 0/, 1/, ... wird zu allen davon aufgefaltet. Modelle aus "
       "verschiedenen Merkmalen lassen sich nicht zusammenführen."),
    FR("Fusionne les modèles sur les images qu'ils partagent : celles-ci donnent "
       "la transformation de similitude entre les deux jauges (D43). Un dossier "
       "contenant 0/, 1/, ... est développé en tous ceux-là. Des modèles bâtis "
       "sur des points différents ne peuvent pas être fusionnés."),
    ES("Fusiona los modelos sobre las imágenes que comparten: estas dan la "
       "transformación de semejanza entre los dos marcos (D43). Una carpeta que "
       "contenga 0/, 1/, ... se despliega en todos ellos. Los modelos "
       "construidos con rasgos distintos no pueden fusionarse."),
    PT("Funde os modelos sobre as imagens que partilham: estas dão a "
       "transformação de semelhança entre os dois referenciais (D43). Uma pasta "
       "que contenha 0/, 1/, ... é expandida para todos eles. Modelos "
       "construídos a partir de traços diferentes não podem ser fundidos."),
    IT("Fonde i modelli sulle immagini che condividono: queste danno la "
       "trasformazione di similitudine tra i due riferimenti (D43). Una cartella "
       "che contiene 0/, 1/, ... viene espansa a tutti quelli. Modelli costruiti "
       "da punti diversi non si possono fondere."),
    NL("Voegt modellen samen op de beelden die ze delen: die geven de "
       "gelijkvormigheidstransformatie tussen de twee ijkingen (D43). Een map "
       "met 0/, 1/, ... wordt uitgevouwen tot al die modellen. Modellen gebouwd "
       "op verschillende kenmerken kunnen niet samengevoegd worden."),
    RU("Сливает модели по изображениям, которые у них общие: те задают "
       "преобразование подобия между двумя калибровками (D43). Каталог, "
       "содержащий 0/, 1/, ..., разворачивается во все из них. Модели, "
       "построенные на разных признаках, слить нельзя."),
    TR("Modelleri paylaştıkları görüntüler üzerinde birleştirir: bunlar iki ölçek "
       "arasındaki benzerlik dönüşümünü verir (D43). 0/, 1/, ... içeren bir dizin "
       "hepsine açılır. Farklı özniteliklerden kurulmuş modeller "
       "birleştirilemez."));

SS_MSG(desc_merge_2,
    EN("A merged model is two independently optimized halves glued along a seam "
       "no bundle adjustment has ever seen, so one runs across it afterwards "
       "unless --no-ba."),
    JA("統合されたモデルは、別々に最適化された 2 つの半分を、どのバンドル調整も"
       "見たことのないシームで貼り合わせたものです。そのため --no-ba を指定しない限り、"
       "あとからシームをまたぐ調整を 1 回走らせます。"),
    ZH_HANS("合并后的模型是两半各自优化的结果，沿着一条任何平差都不曾见过的接缝拼在一起；"
            "因此除非指定 --no-ba，之后会跨接缝再做一次平差。"),
    ZH_HANT("合併後的模型是兩半各自最佳化的結果，沿著一條任何平差都不曾見過的接縫拼在一起；"
            "因此除非指定 --no-ba，之後會跨接縫再做一次平差。"),
    KO("병합된 모델은 따로 최적화된 두 반쪽을, 어떤 번들 조정도 본 적 없는 솔기를 따라 "
       "붙인 것입니다. 그래서 --no-ba 를 주지 않는 한 뒤에 솔기를 가로지르는 조정을 "
       "한 번 돌립니다."),
    DE("Ein vereintes Modell besteht aus zwei unabhängig optimierten Hälften, "
       "entlang einer Naht verklebt, die kein Bündelausgleich je gesehen hat; "
       "darum läuft danach einer darüber, sofern nicht --no-ba."),
    FR("Un modèle fusionné, ce sont deux moitiés optimisées séparément et collées "
       "le long d'une couture qu'aucun ajustement n'a jamais vue ; on en fait "
       "donc passer un dessus ensuite, sauf --no-ba."),
    ES("Un modelo fusionado son dos mitades optimizadas por separado y pegadas a "
       "lo largo de una costura que ningún ajuste ha visto nunca, así que "
       "después se pasa uno por encima, salvo con --no-ba."),
    PT("Um modelo fundido são duas metades otimizadas em separado e coladas ao "
       "longo de uma costura que nenhum ajuste jamais viu, por isso passa-se um "
       "por cima depois, salvo com --no-ba."),
    IT("Un modello fuso è fatto di due metà ottimizzate a parte e incollate lungo "
       "una cucitura che nessun aggiustamento ha mai visto, perciò dopo se ne "
       "passa uno sopra, a meno di --no-ba."),
    NL("Een samengevoegd model is twee apart geoptimaliseerde helften, aan elkaar "
       "geplakt langs een naad die geen enkele bundelaanpassing ooit zag; daarom "
       "gaat er daarna een overheen, tenzij --no-ba."),
    RU("Слитая модель -- это две независимо оптимизированные половины, склеенные "
       "по шву, которого не видело ни одно уравнивание, поэтому потом по нему "
       "проходит одно, если не задан --no-ba."),
    TR("Birleştirilmiş bir model, ayrı ayrı optimize edilmiş iki yarının, hiçbir "
       "demet dengelemesinin görmediği bir dikiş boyunca yapıştırılmasıdır; bu "
       "yüzden --no-ba verilmedikçe sonradan üzerinden bir tane geçirilir."));

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

SS_MSG(opt_progress_dir,
    EN("Write snapshots of the run into DIR while it goes -- the current model "
       "and the pair matrix -- for a front end that is watching. Off by "
       "default; the graphical interface passes it to its own child."),
    JA("実行中の様子を DIR に書き出します。今の再構成と画像ペアの対応表で、"
       "画面で見ているものが読み取ります。既定では書き出しません。"
       "グラフィカル版が自分の子プロセスに渡します。"),
    ZH_HANS("运行时把过程快照写入 DIR —— 当前的重建结果和图像配对表 —— 供正在观察的"
            "界面读取。默认不写。图形界面会把它传给自己的子进程。"),
    ZH_HANT("執行時把過程快照寫入 DIR —— 目前的重建結果和影像配對表 —— 供正在觀察的"
            "介面讀取。預設不寫。圖形介面會把它傳給自己的子行程。"),
    KO("실행 중의 상태를 DIR 에 씁니다. 지금까지의 재구성과 이미지 짝 표로, "
       "화면에서 지켜보는 쪽이 읽습니다. 기본값은 쓰지 않기입니다. "
       "그래픽 화면이 자기 자식 프로세스에 넘깁니다."),
    DE("Während des Laufs Momentaufnahmen nach DIR schreiben -- das aktuelle "
       "Modell und die Paarmatrix -- für eine Oberfläche, die zusieht. "
       "Standardmäßig aus; die grafische Oberfläche gibt es ihrem eigenen "
       "Kindprozess mit."),
    FR("Écrire des instantanés de l'exécution dans DIR au fur et à mesure -- le "
       "modèle courant et la matrice des paires -- pour une interface qui "
       "regarde. Désactivé par défaut ; l'interface graphique le passe à son "
       "propre processus enfant."),
    ES("Escribir instantáneas de la ejecución en DIR sobre la marcha (el modelo "
       "actual y la matriz de pares) para una interfaz que está mirando. "
       "Desactivado por omisión; la interfaz gráfica se lo pasa a su propio "
       "proceso hijo."),
    PT("Escrever instantâneos da execução em DIR conforme ela anda -- o modelo "
       "atual e a matriz de pares -- para uma interface que está olhando. "
       "Desligado por padrão; a interface gráfica passa isso ao próprio "
       "processo filho."),
    IT("Scrivere istantanee dell'esecuzione in DIR mentre procede -- il modello "
       "corrente e la matrice delle coppie -- per un'interfaccia che sta "
       "guardando. Disattivo per impostazione predefinita; l'interfaccia "
       "grafica lo passa al proprio processo figlio."),
    NL("Tijdens de run momentopnamen naar DIR schrijven -- het huidige model en "
       "de parenmatrix -- voor een schil die meekijkt. Standaard uit; de "
       "grafische schil geeft het aan zijn eigen kindproces mee."),
    RU("По ходу работы писать снимки в DIR -- текущую модель и матрицу пар -- "
       "для оболочки, которая за этим следит. По умолчанию выключено; "
       "графическая оболочка передаёт это своему дочернему процессу."),
    TR("Çalışma sürerken anlık görüntüleri DIR içine yaz -- şu anki model ve "
       "çift dizeyi -- izleyen bir arayüz için. Öntanımlı olarak kapalı; "
       "grafik arayüz bunu kendi alt sürecine geçirir."));

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
    EN("Audit every pose against the correspondence graph before anything else, "
       "and re-register what the model cannot support. Use with --resume on "
       "models from elsewhere."),
    JA("何よりも先に、すべての姿勢を対応グラフに照らして監査し、モデルが支持できない"
       "ものを登録し直します。よそから持ってきたモデルには --resume と併せて"
       "使ってください。"),
    ZH_HANS("先于一切，对照对应关系图审计每一个位姿，并把模型无法支撑的重新注册。"
            "对来自别处的模型请与 --resume 一同使用。"),
    ZH_HANT("先於一切，對照對應關係圖稽核每一個位姿，並把模型無法支撐的重新註冊。"
            "對來自別處的模型請與 --resume 一同使用。"),
    KO("무엇보다 먼저 모든 자세를 대응 그래프에 비추어 감사하고, 모델이 뒷받침하지 "
       "못하는 것을 다시 등록합니다. 다른 곳에서 온 모델에는 --resume 과 함께 "
       "쓰세요."),
    DE("Vor allem anderen jede Pose gegen den Korrespondenzgraphen prüfen und neu "
       "registrieren, was das Modell nicht tragen kann. Bei Modellen von "
       "anderswo zusammen mit --resume verwenden."),
    FR("Auditer chaque pose contre le graphe de correspondances avant toute "
       "chose, et réenregistrer ce que le modèle ne peut soutenir. À employer "
       "avec --resume sur des modèles venus d'ailleurs."),
    ES("Auditar cada pose contra el grafo de correspondencias antes que nada, y "
       "volver a registrar lo que el modelo no puede sostener. Úsese con "
       "--resume en modelos de otra procedencia."),
    PT("Auditar cada pose contra o grafo de correspondências antes de tudo, e "
       "registrar de novo o que o modelo não consegue sustentar. Use com "
       "--resume em modelos vindos de outro lugar."),
    IT("Verificare ogni posa rispetto al grafo delle corrispondenze prima di "
       "ogni altra cosa, e registrare di nuovo ciò che il modello non regge. Da "
       "usare con --resume su modelli venuti da altrove."),
    NL("Voor alles elke pose toetsen aan de correspondentiegraaf en opnieuw "
       "registreren wat het model niet kan dragen. Gebruik dit met --resume bij "
       "modellen van elders."),
    RU("Прежде всего сверить каждую позу с графом соответствий и "
       "перерегистрировать то, что модель не выдерживает. Применяйте вместе с "
       "--resume к моделям со стороны."),
    TR("Her şeyden önce her duruşu karşılık çizgesine göre denetle ve modelin "
       "destekleyemediğini yeniden kaydet. Başka yerden gelen modellerde "
       "--resume ile birlikte kullanın."));

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
// `spirula sfm ba` -- the solver benchmark
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
    EN("Runs the GPU bundle adjuster directly on a problem in Bundle Adjustment "
       "in the Large format, and reports cost, iterations, time and VRAM. This "
       "is how the solver is benchmarked and debugged against a published "
       "reference; the pipeline itself never reads BAL. See src/sfm/ba/README.md."),
    JA("Bundle Adjustment in the Large 形式の問題に対して GPU のバンドル調整器を"
       "直接走らせ、コスト・反復回数・時間・VRAM を報告します。公開されている"
       "リファレンスと突き合わせてソルバーをベンチマークし、デバッグするための"
       "ものです。パイプライン自体が BAL を読むことはありません。"
       "src/sfm/ba/README.md を参照してください。"),
    ZH_HANS("直接对 Bundle Adjustment in the Large 格式的问题运行 GPU 平差器，"
            "并报告代价、迭代次数、耗时与显存。这是拿已发表的参考实现来给求解器"
            "做基准测试和排错的方式；流程本身从不读取 BAL。参见 "
            "src/sfm/ba/README.md。"),
    ZH_HANT("直接對 Bundle Adjustment in the Large 格式的問題執行 GPU 平差器，"
            "並回報代價、疊代次數、耗時與顯示記憶體。這是拿已發表的參考實作來給求解器"
            "做基準測試和除錯的方式；流程本身從不讀取 BAL。參見 "
            "src/sfm/ba/README.md。"),
    KO("Bundle Adjustment in the Large 형식의 문제에 GPU 번들 조정기를 곧바로 "
       "돌리고 비용, 반복 횟수, 시간, VRAM 을 알려 줍니다. 공개된 참조 구현과 "
       "견주어 솔버를 벤치마크하고 디버깅하는 방법이며, 파이프라인 자체는 BAL 을 "
       "읽지 않습니다. src/sfm/ba/README.md 를 보세요."),
    DE("Lässt den GPU-Bündelausgleicher unmittelbar auf einem Problem im Format "
       "Bundle Adjustment in the Large laufen und meldet Kosten, Iterationen, "
       "Zeit und VRAM. So wird der Löser gegen eine veröffentlichte Referenz "
       "gemessen und geprüft; die Pipeline selbst liest nie BAL. Siehe "
       "src/sfm/ba/README.md."),
    FR("Fait tourner l'ajusteur de faisceaux GPU directement sur un problème au "
       "format Bundle Adjustment in the Large, et rapporte coût, itérations, "
       "temps et VRAM. C'est ainsi que le solveur est étalonné et débogué face à "
       "une référence publiée ; la chaîne elle-même ne lit jamais BAL. Voir "
       "src/sfm/ba/README.md."),
    ES("Ejecuta el ajustador de haces en la GPU directamente sobre un problema "
       "en formato Bundle Adjustment in the Large, e informa de coste, "
       "iteraciones, tiempo y VRAM. Así se mide y se depura el solucionador "
       "frente a una referencia publicada; la cadena en sí nunca lee BAL. Véase "
       "src/sfm/ba/README.md."),
    PT("Executa o ajustador de feixes na GPU diretamente sobre um problema no "
       "formato Bundle Adjustment in the Large, e relata custo, iterações, tempo "
       "e VRAM. É assim que o solucionador é medido e depurado face a uma "
       "referência publicada; a cadeia em si nunca lê BAL. Veja "
       "src/sfm/ba/README.md."),
    IT("Esegue l'aggiustatore di fasci su GPU direttamente su un problema in "
       "formato Bundle Adjustment in the Large, e riferisce costo, iterazioni, "
       "tempo e VRAM. È così che il risolutore viene misurato e messo a punto "
       "contro un riferimento pubblicato; la catena stessa non legge mai BAL. "
       "Veda src/sfm/ba/README.md."),
    NL("Draait de GPU-bundelaanpasser rechtstreeks op een probleem in het "
       "formaat Bundle Adjustment in the Large, en meldt kosten, iteraties, tijd "
       "en VRAM. Zo wordt de oplosser gemeten en nagelopen tegen een "
       "gepubliceerde referentie; de keten zelf leest nooit BAL. Zie "
       "src/sfm/ba/README.md."),
    RU("Запускает уравниватель на GPU прямо на задаче в формате Bundle "
       "Adjustment in the Large и сообщает стоимость, число итераций, время и "
       "видеопамять. Так решатель сверяют с опубликованным эталоном и отлаживают; "
       "сам конвейер BAL никогда не читает. См. src/sfm/ba/README.md."),
    TR("GPU demet dengeleyicisini, Bundle Adjustment in the Large biçimindeki "
       "bir problem üzerinde doğrudan çalıştırır ve maliyeti, yinelemeleri, "
       "süreyi ve VRAM'i bildirir. Çözücü, yayımlanmış bir referansa karşı böyle "
       "kıyaslanır ve ayıklanır; işlem hattının kendisi BAL'ı hiç okumaz. Bkz. "
       "src/sfm/ba/README.md."));

SS_MSG(ba_desc_2,
    EN("Given a directory instead, it reads a COLMAP sparse model and runs "
       "exactly the global BA the mapper runs on it (Huber 2 px unless --loss "
       "says otherwise), which is how the solver is profiled on real captures. "
       "-o writes the refined model."),
    JA("代わりにディレクトリを渡すと COLMAP の疎なモデルを読み込み、マッパーが"
       "そのモデルに対して行うのと同じ全体バンドル調整を実行します"
       "（--loss の指定がなければ Huber 2 px）。実際の撮影でソルバーを"
       "プロファイルする方法です。-o を付けると精密化したモデルを書き出します。"),
    ZH_HANS("若改为传入一个目录，它会读取 COLMAP 稀疏模型，并对其执行与建图器完全"
            "相同的全局平差（除非 --loss 另有指定，否则用 Huber 2 px）；"
            "这是在真实拍摄上给求解器做性能分析的方式。-o 会写出精化后的模型。"),
    ZH_HANT("若改為傳入一個目錄，它會讀取 COLMAP 稀疏模型，並對其執行與建圖器完全"
            "相同的全域平差（除非 --loss 另有指定，否則用 Huber 2 px）；"
            "這是在真實拍攝上給求解器做效能分析的方式。-o 會寫出精化後的模型。"),
    KO("대신 디렉터리를 주면 COLMAP 성긴 모델을 읽어, 매퍼가 그 모델에 돌리는 것과 "
       "똑같은 전역 번들 조정을 실행합니다(--loss 로 달리 말하지 않으면 Huber 2 px). "
       "실제 촬영에서 솔버를 프로파일링하는 방법입니다. -o 는 정련된 모델을 씁니다."),
    DE("Bekommt es stattdessen ein Verzeichnis, liest es ein dünnes COLMAP-Modell "
       "und führt genau den globalen Ausgleich aus, den der Kartierer darauf "
       "ausführt (Huber 2 px, sofern --loss nichts anderes sagt) -- so wird der "
       "Löser an echten Aufnahmen vermessen. -o schreibt das verfeinerte Modell."),
    FR("Si on lui donne plutôt un dossier, il lit un modèle épars COLMAP et "
       "exécute exactement l'ajustement global que le cartographe y applique "
       "(Huber 2 px sauf indication de --loss), ce qui permet de profiler le "
       "solveur sur de vraies prises de vue. -o écrit le modèle affiné."),
    ES("Si en su lugar se le da una carpeta, lee un modelo disperso de COLMAP y "
       "ejecuta exactamente el ajuste global que el cartógrafo le aplica (Huber "
       "2 px salvo que --loss diga otra cosa), que es como se perfila el "
       "solucionador sobre capturas reales. -o escribe el modelo refinado."),
    PT("Se em vez disso receber uma pasta, lê um modelo esparso do COLMAP e "
       "executa exatamente o ajuste global que o cartógrafo lhe aplica (Huber "
       "2 px salvo se --loss disser outra coisa), que é como o solucionador é "
       "perfilado em capturas reais. -o escreve o modelo refinado."),
    IT("Se invece riceve una cartella, legge un modello sparso COLMAP ed esegue "
       "esattamente il bundle adjustment globale che il cartografo vi applica "
       "(Huber 2 px salvo diverso --loss), ed è così che il risolutore viene "
       "profilato su riprese reali. -o scrive il modello affinato."),
    NL("Krijgt het in plaats daarvan een map, dan leest het een ijl COLMAP-model "
       "en draait precies de globale aanpassing die de kaartmaker erop draait "
       "(Huber 2 px tenzij --loss anders zegt), waarmee de oplosser op echte "
       "opnamen wordt doorgemeten. -o schrijft het verfijnde model."),
    RU("Если же ему дать каталог, он прочитает разреженную модель COLMAP и "
       "выполнит ровно то глобальное уравнивание, которое проводит по ней "
       "построитель (Huber 2 px, если --loss не скажет иначе) -- так решатель "
       "профилируют на настоящих съёмках. -o записывает уточнённую модель."),
    TR("Bunun yerine bir dizin verilirse, bir COLMAP seyrek modelini okur ve "
       "haritalayıcının onun üzerinde çalıştırdığı genel dengelemenin tam olarak "
       "aynısını çalıştırır (--loss aksini söylemedikçe Huber 2 px); çözücü "
       "gerçek çekimlerde böyle profillenir. -o iyileştirilmiş modeli yazar."));

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

SS_MSG(ba_opt_ply,
    EN("write PREFIX_before.ply and PREFIX_after.ply"),
    JA("PREFIX_before.ply と PREFIX_after.ply を書き出します"),
    ZH_HANS("写出 PREFIX_before.ply 与 PREFIX_after.ply"),
    ZH_HANT("寫出 PREFIX_before.ply 與 PREFIX_after.ply"),
    KO("PREFIX_before.ply 와 PREFIX_after.ply 를 씁니다"),
    DE("PREFIX_before.ply und PREFIX_after.ply schreiben"),
    FR("écrire PREFIX_before.ply et PREFIX_after.ply"),
    ES("escribir PREFIX_before.ply y PREFIX_after.ply"),
    PT("escrever PREFIX_before.ply e PREFIX_after.ply"),
    IT("scrivere PREFIX_before.ply e PREFIX_after.ply"),
    NL("PREFIX_before.ply en PREFIX_after.ply schrijven"),
    RU("записать PREFIX_before.ply и PREFIX_after.ply"),
    TR("PREFIX_before.ply ve PREFIX_after.ply yaz"));

SS_MSG(ba_opt_output,
    EN("write the refined sparse model (model input only)"),
    JA("精密化した疎なモデルを書き出します（入力がモデルの場合のみ）"),
    ZH_HANS("写出精化后的稀疏模型（仅当输入是模型时）"),
    ZH_HANT("寫出精化後的稀疏模型（僅當輸入是模型時）"),
    KO("정련한 성긴 모델을 씁니다(입력이 모델일 때만)"),
    DE("das verfeinerte dünne Modell schreiben (nur bei Modelleingabe)"),
    FR("écrire le modèle épars affiné (entrée modèle seulement)"),
    ES("escribir el modelo disperso refinado (solo con entrada de modelo)"),
    PT("escrever o modelo esparso refinado (só com entrada de modelo)"),
    IT("scrivere il modello sparso affinato (solo con ingresso modello)"),
    NL("het verfijnde ijle model schrijven (alleen bij modelinvoer)"),
    RU("записать уточнённую разреженную модель (только если на входе модель)"),
    TR("iyileştirilmiş seyrek modeli yaz (yalnızca model girdisinde)"));

SS_MSG(ba_opt_device,
    EN("Vulkan device index"),
    JA("Vulkan デバイスの番号"),
    ZH_HANS("Vulkan 设备序号"),
    ZH_HANT("Vulkan 裝置序號"),
    KO("Vulkan 장치 번호"),
    DE("Index des Vulkan-Geräts"),
    FR("indice du périphérique Vulkan"),
    ES("índice del dispositivo Vulkan"),
    PT("índice do dispositivo Vulkan"),
    IT("indice del dispositivo Vulkan"),
    NL("index van het Vulkan-apparaat"),
    RU("номер устройства Vulkan"),
    TR("Vulkan aygıt sırası"));

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

}  // namespace sfmhelp
}  // namespace msg
}  // namespace i18n
}  // namespace spirula

#include "i18n/EndCatalog.h"
