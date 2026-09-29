#pragma once

// What `spirula partition` prints and what the GUI's Partition panel says:
// splitting a reconstruction into parts that train separately, and merging
// the parts' models back into one. Paths, flag names and the words `core`
// and `ring` as identifiers stay as they are in every language.

#include "i18n/BeginCatalog.h"

namespace spirula {
namespace i18n {
namespace msg {
namespace partition {

// ===========================================================================
// spirula partition --help
// ===========================================================================

SS_MSG(tagline,
    EN("Split a reconstruction into parts that train separately, and merge the models back"),
    JA("再構成を別々に学習するパートに分割し、モデルをひとつに結合します"),
    ZH_HANS("把重建拆成可分别训练的分区，再把模型合并回来"),
    ZH_HANT("把重建拆成可分別訓練的分區，再把模型合併回來"),
    KO("재구성을 따로 학습할 파트로 나누고, 모델을 다시 하나로 병합합니다"),
    DE("Eine Rekonstruktion in getrennt trainierbare Teile zerlegen und die Modelle wieder zusammenführen"),
    FR("Découper une reconstruction en parties entraînées séparément, puis refusionner les modèles"),
    ES("Dividir una reconstrucción en partes que se entrenan por separado y fusionar los modelos"),
    PT("Dividir uma reconstrução em partes treinadas em separado e voltar a fundir os modelos"),
    IT("Dividere una ricostruzione in parti addestrate separatamente e riunire i modelli"),
    NL("Een reconstructie splitsen in apart getrainde delen en de modellen weer samenvoegen"),
    RU("Разбить реконструкцию на части, обучаемые раздельно, и объединить модели обратно"),
    TR("Bir yeniden oluşturmayı ayrı eğitilen parçalara bölmek ve modelleri geri birleştirmek"));

SS_MSG(usage_intro,
    EN("`split` reads a COLMAP, Nerfstudio or Metashape dataset, cuts its cameras "
       "where they share the least, gives every part the region its cameras see "
       "and a ring of outside cameras that see it too, and writes partition.json. "
       "Train each part with `spirula train <dataset> --partition <file> "
       "--partition-part <k>`, then `merge` keeps each model's splats inside its "
       "own region and writes them as one file."),
    JA("`split` は COLMAP / Nerfstudio / Metashape のデータセットを読み、カメラを"
       "共有の最も少ない所で切り、各パートにそのカメラが見る領域と、それを見てい"
       "る外側のカメラの輪を与えて partition.json を書き出します。各パートは "
       "`spirula train <dataset> --partition <file> --partition-part <k>` で学習"
       "し、`merge` が各モデルのスプラットを自分の領域内だけ残してひとつのファイ"
       "ルにまとめます。"),
    ZH_HANS("`split` 读取 COLMAP、Nerfstudio 或 Metashape 数据集，在相机共视最少"
            "处切开，给每个分区分配其相机所见的区域和同样看到该区域的外围相机环，"
            "写出 partition.json。用 `spirula train <dataset> --partition <file> "
            "--partition-part <k>` 训练各分区，再用 `merge` 只保留各模型在自身区"
            "域内的泼溅并写成一个文件。"),
    ZH_HANT("`split` 讀取 COLMAP、Nerfstudio 或 Metashape 資料集，在相機共視最少"
            "處切開，給每個分區分配其相機所見的區域和同樣看到該區域的外圍相機環，"
            "寫出 partition.json。用 `spirula train <dataset> --partition <file> "
            "--partition-part <k>` 訓練各分區，再用 `merge` 只保留各模型在自身區"
            "域內的潑濺並寫成一個檔案。"),
    KO("`split`은 COLMAP, Nerfstudio 또는 Metashape 데이터셋을 읽어 카메라를 가장 "
       "적게 공유하는 곳에서 잘라, 각 파트에 그 카메라가 보는 영역과 그 영역을 함께 "
       "보는 바깥 카메라의 고리를 주고 partition.json을 씁니다. 각 파트는 `spirula "
       "train <dataset> --partition <file> --partition-part <k>`로 학습하고, `merge`"
       "가 각 모델의 스플랫을 자기 영역 안의 것만 남겨 한 파일로 씁니다."),
    DE("`split` liest einen COLMAP-, Nerfstudio- oder Metashape-Datensatz, "
       "schneidet die Kameras dort, wo sie am wenigsten gemeinsam sehen, gibt "
       "jedem Teil den Bereich, den seine Kameras sehen, und einen Ring äußerer "
       "Kameras, die ihn mitsehen, und schreibt partition.json. Jeder Teil wird "
       "mit `spirula train <dataset> --partition <file> --partition-part <k>` "
       "trainiert; `merge` behält von jedem Modell die Splats im eigenen Bereich "
       "und schreibt sie als eine Datei."),
    FR("`split` lit un jeu de données COLMAP, Nerfstudio ou Metashape, coupe les "
       "caméras là où elles partagent le moins, donne à chaque partie la région "
       "que voient ses caméras et un anneau de caméras extérieures qui la voient "
       "aussi, et écrit partition.json. Entraînez chaque partie avec `spirula "
       "train <dataset> --partition <file> --partition-part <k>`, puis `merge` "
       "garde de chaque modèle les splats de sa propre région et les écrit en un "
       "seul fichier."),
    ES("`split` lee un conjunto COLMAP, Nerfstudio o Metashape, corta las cámaras "
       "donde menos comparten, da a cada parte la región que ven sus cámaras y un "
       "anillo de cámaras externas que también la ven, y escribe partition.json. "
       "Entrena cada parte con `spirula train <dataset> --partition <file> "
       "--partition-part <k>`; después `merge` conserva de cada modelo los splats "
       "dentro de su región y los escribe en un solo archivo."),
    PT("`split` lê um conjunto COLMAP, Nerfstudio ou Metashape, corta as câmaras "
       "onde menos partilham, dá a cada parte a região que as suas câmaras veem e "
       "um anel de câmaras exteriores que também a veem, e escreve partition.json. "
       "Treine cada parte com `spirula train <dataset> --partition <file> "
       "--partition-part <k>`; depois `merge` guarda de cada modelo os splats "
       "dentro da sua região e escreve-os num só ficheiro."),
    IT("`split` legge un insieme COLMAP, Nerfstudio o Metashape, taglia le "
       "fotocamere dove condividono meno, dà a ogni parte la regione che le sue "
       "fotocamere vedono e un anello di fotocamere esterne che la vedono anche "
       "loro, e scrive partition.json. Addestra ogni parte con `spirula train "
       "<dataset> --partition <file> --partition-part <k>`; poi `merge` tiene di "
       "ogni modello gli splat dentro la propria regione e li scrive in un solo "
       "file."),
    NL("`split` leest een COLMAP-, Nerfstudio- of Metashape-dataset, snijdt de "
       "camera's waar ze het minst delen, geeft elk deel het gebied dat zijn "
       "camera's zien plus een ring buitencamera's die het ook zien, en schrijft "
       "partition.json. Train elk deel met `spirula train <dataset> --partition "
       "<file> --partition-part <k>`; daarna houdt `merge` van elk model de splats "
       "binnen het eigen gebied en schrijft ze als één bestand."),
    RU("`split` читает набор COLMAP, Nerfstudio или Metashape, режет камеры там, "
       "где они меньше всего пересекаются, даёт каждой части область, которую "
       "видят её камеры, и кольцо внешних камер, которые тоже её видят, и пишет "
       "partition.json. Каждую часть обучают `spirula train <dataset> --partition "
       "<file> --partition-part <k>`, затем `merge` оставляет от каждой модели "
       "сплаты внутри её области и записывает их одним файлом."),
    TR("`split` bir COLMAP, Nerfstudio ya da Metashape veri kümesini okur, "
       "kameraları en az paylaştıkları yerden keser, her parçaya kameralarının "
       "gördüğü bölgeyi ve onu da gören dış kameralardan bir halka verir ve "
       "partition.json yazar. Her parçayı `spirula train <dataset> --partition "
       "<file> --partition-part <k>` ile eğitin; sonra `merge` her modelin kendi "
       "bölgesindeki splatlarını tutup tek dosya yazar."));

SS_MSG(head_options,
    EN("Options"), JA("オプション"), ZH_HANS("选项"), ZH_HANT("選項"),
    KO("옵션"), DE("Optionen"), FR("Options"), ES("Opciones"), PT("Opções"),
    IT("Opzioni"), NL("Opties"), RU("Параметры"), TR("Seçenekler"));

SS_MSG(head_split,
    EN("split options"), JA("split のオプション"), ZH_HANS("split 选项"),
    ZH_HANT("split 選項"), KO("split 옵션"), DE("Optionen für split"),
    FR("options de split"), ES("opciones de split"), PT("opções de split"),
    IT("opzioni di split"), NL("opties van split"), RU("параметры split"),
    TR("split seçenekleri"));

SS_MSG(head_merge,
    EN("merge options"), JA("merge のオプション"), ZH_HANS("merge 选项"),
    ZH_HANT("merge 選項"), KO("merge 옵션"), DE("Optionen für merge"),
    FR("options de merge"), ES("opciones de merge"), PT("opções de merge"),
    IT("opzioni di merge"), NL("opties van merge"), RU("параметры merge"),
    TR("merge seçenekleri"));

SS_MSG(opt_parts,
    EN("How many parts. 0 splits until every part has at most --max-images "
       "cameras. Default: 0"),
    JA("パートの数。0 なら各パートのカメラが --max-images 以下になるまで分割しま"
       "す。既定: 0"),
    ZH_HANS("分区数量。0 表示一直拆到每个分区的相机数不超过 --max-images。默认：0"),
    ZH_HANT("分區數量。0 表示一直拆到每個分區的相機數不超過 --max-images。預設：0"),
    KO("파트 수. 0이면 모든 파트의 카메라가 --max-images 이하가 될 때까지 나눕니다. "
       "기본값: 0"),
    DE("Wie viele Teile. 0 teilt, bis jeder Teil höchstens --max-images Kameras "
       "hat. Standard: 0"),
    FR("Nombre de parties. 0 découpe jusqu'à ce que chaque partie ait au plus "
       "--max-images caméras. Défaut : 0"),
    ES("Cuántas partes. 0 divide hasta que cada parte tenga como máximo "
       "--max-images cámaras. Predeterminado: 0"),
    PT("Quantas partes. 0 divide até cada parte ter no máximo --max-images "
       "câmaras. Predefinição: 0"),
    IT("Quante parti. 0 divide finché ogni parte ha al massimo --max-images "
       "fotocamere. Predefinito: 0"),
    NL("Hoeveel delen. 0 splitst tot elk deel hoogstens --max-images camera's "
       "heeft. Standaard: 0"),
    RU("Сколько частей. 0 делит, пока в каждой части не останется не больше "
       "--max-images камер. По умолчанию: 0"),
    TR("Kaç parça. 0, her parçada en çok --max-images kamera kalana dek böler. "
       "Varsayılan: 0"));

SS_MSG(opt_max_images,
    EN("Most cameras a part trains with, core and ring, when --parts is 0. "
       "Default: 2000"),
    JA("--parts が 0 のときの、パートが学習に使うカメラ（コアとリング）の上限。既定: 2000"),
    ZH_HANS("--parts 为 0 时每个分区训练所用相机（核心加环）的上限。默认：2000"),
    ZH_HANT("--parts 為 0 時每個分區訓練所用相機（核心加環）的上限。預設：2000"),
    KO("--parts가 0일 때 파트가 학습에 쓰는 카메라(핵심과 링) 상한. 기본값: 2000"),
    DE("Höchstzahl an Kameras, Kern und Ring, mit denen ein Teil trainiert, wenn "
       "--parts 0 ist. Standard: 2000"),
    FR("Nombre maximal de caméras, cœur et anneau, avec lesquelles une partie "
       "s'entraîne quand --parts vaut 0. Défaut : 2000"),
    ES("Máximo de cámaras, núcleo y anillo, con las que entrena una parte cuando "
       "--parts es 0. Predeterminado: 2000"),
    PT("Máximo de câmaras, núcleo e anel, com que uma parte treina quando --parts "
       "é 0. Predefinição: 2000"),
    IT("Massimo di fotocamere, nucleo e anello, con cui una parte si addestra "
       "quando --parts è 0. Predefinito: 2000"),
    NL("Meeste camera's, kern en ring, waarmee een deel traint als --parts 0 is. "
       "Standaard: 2000"),
    RU("Наибольшее число камер, ядро и кольцо, на которых обучается часть, когда "
       "--parts равно 0. По умолчанию: 2000"),
    TR("--parts 0 iken bir parçanın eğitimde kullandığı en çok kamera sayısı, "
       "çekirdek ve halka. Varsayılan: 2000"));

SS_MSG(opt_ring,
    EN("A camera outside a part joins its ring when at least this share of the "
       "points it sees belongs to the part. Default: 0.1"),
    JA("パート外のカメラは、見ている点のうちこの割合以上がそのパートに属するとき"
       "リングに加わります。既定: 0.1"),
    ZH_HANS("分区外的相机所见点中属于该分区的比例达到此值时，加入其外环。默认：0.1"),
    ZH_HANT("分區外的相機所見點中屬於該分區的比例達到此值時，加入其外環。預設：0.1"),
    KO("파트 밖의 카메라는 자신이 보는 점 중 이 비율 이상이 그 파트에 속할 때 고리에 "
       "들어갑니다. 기본값: 0.1"),
    DE("Eine Kamera außerhalb eines Teils tritt seinem Ring bei, wenn mindestens "
       "dieser Anteil der von ihr gesehenen Punkte zum Teil gehört. Standard: 0.1"),
    FR("Une caméra hors d'une partie rejoint son anneau quand au moins cette part "
       "des points qu'elle voit appartient à la partie. Défaut : 0.1"),
    ES("Una cámara fuera de una parte entra en su anillo cuando al menos esta "
       "fracción de los puntos que ve pertenece a la parte. Predeterminado: 0.1"),
    PT("Uma câmara fora de uma parte entra no seu anel quando pelo menos esta "
       "fração dos pontos que vê pertence à parte. Predefinição: 0.1"),
    IT("Una fotocamera fuori da una parte entra nel suo anello quando almeno "
       "questa quota dei punti che vede appartiene alla parte. Predefinito: 0.1"),
    NL("Een camera buiten een deel komt in zijn ring als minstens dit aandeel van "
       "de punten die ze ziet bij het deel hoort. Standaard: 0.1"),
    RU("Камера вне части входит в её кольцо, если хотя бы такая доля видимых ею "
       "точек принадлежит части. По умолчанию: 0.1"),
    TR("Bir parçanın dışındaki kamera, gördüğü noktaların en az bu payı parçaya "
       "aitse halkasına katılır. Varsayılan: 0.1"));

SS_MSG(opt_ring_min,
    EN("...and at least this many of them. Default: 20"),
    JA("…かつ点の数がこれ以上のとき。既定: 20"),
    ZH_HANS("……且这样的点至少有这么多。默认：20"),
    ZH_HANT("……且這樣的點至少有這麼多。預設：20"),
    KO("...그리고 그런 점이 최소 이만큼일 때. 기본값: 20"),
    DE("... und mindestens so viele davon. Standard: 20"),
    FR("... et au moins ce nombre d'entre eux. Défaut : 20"),
    ES("... y al menos este número de ellos. Predeterminado: 20"),
    PT("... e pelo menos este número deles. Predefinição: 20"),
    IT("... e almeno questo numero di essi. Predefinito: 20"),
    NL("... en minstens zoveel daarvan. Standaard: 20"),
    RU("... и не меньше такого их числа. По умолчанию: 20"),
    TR("... ve bunlardan en az bu kadarı. Varsayılan: 20"));

SS_MSG(opt_max_seeds,
    EN("Seed points the ownership field keeps, strided from the cloud; the "
       "cameras are always in it. Default: 1000000"),
    JA("所有領域フィールドに残す初期点の数（点群から等間隔に抽出）。カメラは常に含"
       "まれます。既定: 1000000"),
    ZH_HANS("归属场保留的种子点数，从点云等距抽取；相机始终包含。默认：1000000"),
    ZH_HANT("歸屬場保留的種子點數，從點雲等距抽取；相機始終包含。預設：1000000"),
    KO("소유 필드가 유지하는 시드 점 수(점 구름에서 등간격 추출). 카메라는 항상 포함"
       "됩니다. 기본값: 1000000"),
    DE("Startpunkte, die das Zugehörigkeitsfeld behält, gleichmäßig aus der Wolke "
       "gezogen; die Kameras sind immer dabei. Standard: 1000000"),
    FR("Points d'amorce gardés par le champ d'appartenance, prélevés à pas "
       "constant dans le nuage ; les caméras y sont toujours. Défaut : 1000000"),
    ES("Puntos semilla que conserva el campo de pertenencia, tomados a paso "
       "constante de la nube; las cámaras siempre están. Predeterminado: 1000000"),
    PT("Pontos semente que o campo de pertença guarda, tirados a passo constante "
       "da nuvem; as câmaras estão sempre lá. Predefinição: 1000000"),
    IT("Punti seme che il campo di appartenenza tiene, presi a passo costante "
       "dalla nuvola; le fotocamere ci sono sempre. Predefinito: 1000000"),
    NL("Zaadpunten die het eigendomsveld bewaart, met vaste stap uit de wolk; de "
       "camera's zitten er altijd in. Standaard: 1000000"),
    RU("Начальных точек, которые хранит поле принадлежности, взятых с шагом из "
       "облака; камеры входят всегда. По умолчанию: 1000000"),
    TR("Aidiyet alanının buluttan sabit adımla tuttuğu tohum noktası sayısı; "
       "kameralar her zaman içindedir. Varsayılan: 1000000"));

SS_MSG(opt_method,
    EN("graph cuts the cameras and the points together where the least view "
       "crosses, so each camera sees most of its part; viewgraph cuts the "
       "cameras alone and takes regions from their positions. Default: graph"),
    JA("graph はカメラと点をまとめて、視野の交差が最も少ない所で切り、各カメラが自分の"
       "パートを最もよく見るようにします。viewgraph はカメラだけを切り、領域は位置から"
       "決めます。既定: graph"),
    ZH_HANS("graph 把相机和点一起在视野交叉最少处切分，让每台相机看到的大多是自己的"
            "分区；viewgraph 只切分相机，区域按相机位置推出。默认：graph"),
    ZH_HANT("graph 把相機和點一起在視野交叉最少處切分，讓每台相機看到的大多是自己的"
            "分區；viewgraph 只切分相機，區域按相機位置推出。預設：graph"),
    KO("graph는 카메라와 점을 함께, 시야가 가장 적게 걸치는 곳에서 잘라 각 카메라가 "
       "자기 파트를 가장 많이 보게 합니다. viewgraph는 카메라만 자르고 영역은 위치에서 "
       "정합니다. 기본값: graph"),
    DE("graph schneidet Kameras und Punkte gemeinsam dort, wo am wenigsten Sicht "
       "die Grenze kreuzt, sodass jede Kamera meist ihren eigenen Teil sieht; "
       "viewgraph schneidet nur die Kameras und nimmt die Bereiche aus ihren "
       "Positionen. Standard: graph"),
    FR("graph coupe caméras et points ensemble là où le moins de vue traverse, "
       "pour que chaque caméra voie surtout sa partie ; viewgraph ne coupe que "
       "les caméras et tire les régions de leurs positions. Défaut : graph"),
    ES("graph corta cámaras y puntos juntos por donde menos vista cruza, para "
       "que cada cámara vea sobre todo su parte; viewgraph corta solo las "
       "cámaras y saca las regiones de sus posiciones. Predeterminado: graph"),
    PT("graph corta câmaras e pontos juntos por onde menos vista atravessa, para "
       "que cada câmara veja sobretudo a sua parte; viewgraph corta só as "
       "câmaras e tira as regiões das suas posições. Predefinição: graph"),
    IT("graph taglia fotocamere e punti insieme dove passa meno vista, così ogni "
       "fotocamera vede soprattutto la propria parte; viewgraph taglia solo le "
       "fotocamere e ricava le regioni dalle loro posizioni. Predefinito: graph"),
    NL("graph snijdt camera's en punten samen waar het minste zicht de grens "
       "kruist, zodat elke camera vooral haar eigen deel ziet; viewgraph snijdt "
       "alleen de camera's en haalt de gebieden uit hun posities. Standaard: graph"),
    RU("graph режет камеры и точки вместе там, где границу пересекает меньше "
       "всего обзора, чтобы каждая камера видела в основном свою часть; viewgraph "
       "режет только камеры, а области берёт из их положений. По умолчанию: graph"),
    TR("graph kameraları ve noktaları birlikte, sınırı en az görüşün kestiği "
       "yerden böler; böylece her kamera çoğunlukla kendi parçasını görür. "
       "viewgraph yalnızca kameraları böler, bölgeleri konumlarından çıkarır. "
       "Varsayılan: graph"));

SS_MSG(opt_source,
    EN("Where covisibility comes from. auto takes the model's own tracks, else "
       "projects the seed cloud into every frame, else uses camera positions "
       "alone. Default: auto"),
    JA("共視性の出どころ。auto はモデル自身のトラックを使い、なければ初期点群を全"
       "フレームに投影し、それもなければカメラ位置だけを使います。既定: auto"),
    ZH_HANS("共视关系的来源。auto 优先用模型自带的轨迹，否则把种子点云投影到每一帧，"
            "再否则只用相机位置。默认：auto"),
    ZH_HANT("共視關係的來源。auto 優先用模型自帶的軌跡，否則把種子點雲投影到每一幀，"
            "再否則只用相機位置。預設：auto"),
    KO("공가시성의 출처. auto는 모델의 트랙을 쓰고, 없으면 시드 점 구름을 모든 "
       "프레임에 투영하며, 그것도 없으면 카메라 위치만 씁니다. 기본값: auto"),
    DE("Woher die Kovisibilität kommt. auto nimmt die Spuren des Modells, sonst "
       "projiziert es die Startpunktwolke in jedes Bild, sonst nur die "
       "Kamerapositionen. Standard: auto"),
    FR("D'où vient la covisibilité. auto prend les pistes du modèle, sinon "
       "projette le nuage d'amorce dans chaque image, sinon n'utilise que les "
       "positions des caméras. Défaut : auto"),
    ES("De dónde sale la covisibilidad. auto toma las pistas del modelo; si no, "
       "proyecta la nube semilla en cada imagen; si no, solo las posiciones de "
       "las cámaras. Predeterminado: auto"),
    PT("De onde vem a covisibilidade. auto usa as pistas do modelo; senão, "
       "projeta a nuvem semente em cada imagem; senão, só as posições das "
       "câmaras. Predefinição: auto"),
    IT("Da dove viene la covisibilità. auto prende le tracce del modello, "
       "altrimenti proietta la nuvola seme in ogni immagine, altrimenti usa solo "
       "le posizioni delle fotocamere. Predefinito: auto"),
    NL("Waar covisibiliteit vandaan komt. auto neemt de sporen van het model, "
       "anders projecteert het de zaadwolk in elk beeld, anders alleen de "
       "cameraposities. Standaard: auto"),
    RU("Откуда берётся совидимость. auto берёт треки модели, иначе проецирует "
       "начальное облако в каждый кадр, иначе использует только положения камер. "
       "По умолчанию: auto"),
    TR("Ortak görünürlüğün kaynağı. auto modelin kendi izlerini alır, yoksa tohum "
       "bulutunu her kareye yansıtır, o da yoksa yalnız kamera konumlarını kullanır. "
       "Varsayılan: auto"));

SS_MSG(opt_output,
    EN("Where to write the partition. Default: <dataset>/partition.json"),
    JA("分割の書き出し先。既定: <dataset>/partition.json"),
    ZH_HANS("分区文件的写出位置。默认：<dataset>/partition.json"),
    ZH_HANT("分區檔案的寫出位置。預設：<dataset>/partition.json"),
    KO("분할을 기록할 위치. 기본값: <dataset>/partition.json"),
    DE("Wohin die Partition geschrieben wird. Standard: <dataset>/partition.json"),
    FR("Où écrire la partition. Défaut : <dataset>/partition.json"),
    ES("Dónde escribir la partición. Predeterminado: <dataset>/partition.json"),
    PT("Onde escrever a partição. Predefinição: <dataset>/partition.json"),
    IT("Dove scrivere la partizione. Predefinito: <dataset>/partition.json"),
    NL("Waar de partitie wordt geschreven. Standaard: <dataset>/partition.json"),
    RU("Куда записать разбиение. По умолчанию: <dataset>/partition.json"),
    TR("Bölümlemenin yazılacağı yer. Varsayılan: <dataset>/partition.json"));

SS_MSG(opt_runs,
    EN("Folder of training runs to find the parts' models in, by their "
       "config.json. Default: <dataset>/outputs"),
    JA("各パートのモデルを config.json から探す学習実行のフォルダ。既定: "
       "<dataset>/outputs"),
    ZH_HANS("根据 config.json 查找各分区模型的训练输出文件夹。默认：<dataset>/outputs"),
    ZH_HANT("根據 config.json 查找各分區模型的訓練輸出資料夾。預設：<dataset>/outputs"),
    KO("config.json으로 각 파트의 모델을 찾을 학습 실행 폴더. 기본값: "
       "<dataset>/outputs"),
    DE("Ordner mit Trainingsläufen, in dem die Modelle der Teile über ihre "
       "config.json gesucht werden. Standard: <dataset>/outputs"),
    FR("Dossier des entraînements où trouver les modèles des parties, via leur "
       "config.json. Défaut : <dataset>/outputs"),
    ES("Carpeta de ejecuciones donde buscar los modelos de las partes por su "
       "config.json. Predeterminado: <dataset>/outputs"),
    PT("Pasta de treinos onde procurar os modelos das partes pelo seu "
       "config.json. Predefinição: <dataset>/outputs"),
    IT("Cartella delle esecuzioni in cui cercare i modelli delle parti dal loro "
       "config.json. Predefinito: <dataset>/outputs"),
    NL("Map met trainingsruns waarin de modellen van de delen via hun config.json "
       "worden gezocht. Standaard: <dataset>/outputs"),
    RU("Папка с запусками обучения, где по config.json ищутся модели частей. "
       "По умолчанию: <dataset>/outputs"),
    TR("Parçaların modellerinin config.json'larından bulunacağı eğitim "
       "çalıştırmaları klasörü. Varsayılan: <dataset>/outputs"));

SS_MSG(opt_merge_output,
    EN("The merged model file. Default: merged.ply beside the partition"),
    JA("結合したモデルのファイル。既定: 分割ファイルの隣の merged.ply"),
    ZH_HANS("合并后的模型文件。默认：分区文件旁的 merged.ply"),
    ZH_HANT("合併後的模型檔案。預設：分區檔案旁的 merged.ply"),
    KO("병합된 모델 파일. 기본값: 분할 파일 옆의 merged.ply"),
    DE("Die zusammengeführte Modelldatei. Standard: merged.ply neben der Partition"),
    FR("Le fichier du modèle fusionné. Défaut : merged.ply à côté de la partition"),
    ES("El archivo del modelo fusionado. Predeterminado: merged.ply junto a la partición"),
    PT("O ficheiro do modelo fundido. Predefinição: merged.ply ao lado da partição"),
    IT("Il file del modello unito. Predefinito: merged.ply accanto alla partizione"),
    NL("Het samengevoegde modelbestand. Standaard: merged.ply naast de partitie"),
    RU("Файл объединённой модели. По умолчанию: merged.ply рядом с разбиением"),
    TR("Birleştirilmiş model dosyası. Varsayılan: bölümlemenin yanındaki merged.ply"));

SS_MSG(opt_merge_part,
    EN("Part k's run folder or splat.ply, instead of searching --runs; "
       "repeatable"),
    JA("--runs を探す代わりに指定するパート k の実行フォルダまたは splat.ply。複"
       "数指定可"),
    ZH_HANS("直接指定分区 k 的运行文件夹或 splat.ply，而不在 --runs 中搜索；可重复"),
    ZH_HANT("直接指定分區 k 的執行資料夾或 splat.ply，而不在 --runs 中搜尋；可重複"),
    KO("--runs를 찾는 대신 파트 k의 실행 폴더나 splat.ply를 직접 지정. 반복 가능"),
    DE("Laufordner oder splat.ply von Teil k statt der Suche in --runs; mehrfach "
       "möglich"),
    FR("Dossier d'entraînement ou splat.ply de la partie k, au lieu de chercher "
       "dans --runs ; répétable"),
    ES("Carpeta de ejecución o splat.ply de la parte k, en lugar de buscar en "
       "--runs; repetible"),
    PT("Pasta de treino ou splat.ply da parte k, em vez de procurar em --runs; "
       "repetível"),
    IT("Cartella di esecuzione o splat.ply della parte k, invece di cercare in "
       "--runs; ripetibile"),
    NL("Runmap of splat.ply van deel k, in plaats van zoeken in --runs; herhaalbaar"),
    RU("Папка запуска или splat.ply части k вместо поиска в --runs; можно "
       "повторять"),
    TR("--runs içinde aramak yerine k parçasının çalıştırma klasörü ya da "
       "splat.ply'si; yinelenebilir"));

SS_MSG(label_common,
    EN("Common:"), JA("共通:"), ZH_HANS("通用："), ZH_HANT("通用："), KO("공통:"),
    DE("Allgemein:"), FR("Commun :"), ES("Común:"), PT("Comum:"), IT("Comune:"),
    NL("Algemeen:"), RU("Общие:"), TR("Ortak:"));

// ===========================================================================
// Errors and log lines
// ===========================================================================

SS_MSG(err_usage,
    EN("Usage: {0} split <dataset> [options]   or   {0} merge <partition.json> [options]"),
    JA("使い方: {0} split <dataset> [options]   または   {0} merge <partition.json> [options]"),
    ZH_HANS("用法：{0} split <dataset> [options]   或   {0} merge <partition.json> [options]"),
    ZH_HANT("用法：{0} split <dataset> [options]   或   {0} merge <partition.json> [options]"),
    KO("사용법: {0} split <dataset> [options]   또는   {0} merge <partition.json> [options]"),
    DE("Aufruf: {0} split <dataset> [options]   oder   {0} merge <partition.json> [options]"),
    FR("Usage : {0} split <dataset> [options]   ou   {0} merge <partition.json> [options]"),
    ES("Uso: {0} split <dataset> [options]   o   {0} merge <partition.json> [options]"),
    PT("Uso: {0} split <dataset> [options]   ou   {0} merge <partition.json> [options]"),
    IT("Uso: {0} split <dataset> [options]   oppure   {0} merge <partition.json> [options]"),
    NL("Gebruik: {0} split <dataset> [options]   of   {0} merge <partition.json> [options]"),
    RU("Использование: {0} split <dataset> [options]   или   {0} merge <partition.json> [options]"),
    TR("Kullanım: {0} split <dataset> [options]   ya da   {0} merge <partition.json> [options]"));

SS_MSG(err_bad_value,
    EN("{0}: not a valid value for {1}"),
    JA("{0}: {1} の値として無効です"),
    ZH_HANS("{0}：不是 {1} 的有效值"),
    ZH_HANT("{0}：不是 {1} 的有效值"),
    KO("{0}: {1}에 쓸 수 없는 값입니다"),
    DE("{0}: kein gültiger Wert für {1}"),
    FR("{0} : valeur invalide pour {1}"),
    ES("{0}: no es un valor válido para {1}"),
    PT("{0}: não é um valor válido para {1}"),
    IT("{0}: non è un valore valido per {1}"),
    NL("{0}: geen geldige waarde voor {1}"),
    RU("{0}: недопустимое значение для {1}"),
    TR("{0}: {1} için geçerli bir değer değil"));

SS_MSG(log_reading,
    EN("Reading {0}"), JA("{0} を読み込み中"), ZH_HANS("正在读取 {0}"),
    ZH_HANT("正在讀取 {0}"), KO("{0} 읽는 중"), DE("Lese {0}"), FR("Lecture de {0}"),
    ES("Leyendo {0}"), PT("A ler {0}"), IT("Lettura di {0}"), NL("{0} wordt gelezen"),
    RU("Чтение {0}"), TR("{0} okunuyor"));

SS_MSG(log_dataset,
    EN("Cameras: {0}   points: {1}   tracks: {2}"),
    JA("カメラ: {0}   点: {1}   トラック: {2}"),
    ZH_HANS("相机：{0}   点：{1}   轨迹：{2}"),
    ZH_HANT("相機：{0}   點：{1}   軌跡：{2}"),
    KO("카메라: {0}   점: {1}   트랙: {2}"),
    DE("Kameras: {0}   Punkte: {1}   Spuren: {2}"),
    FR("Caméras : {0}   points : {1}   pistes : {2}"),
    ES("Cámaras: {0}   puntos: {1}   pistas: {2}"),
    PT("Câmaras: {0}   pontos: {1}   pistas: {2}"),
    IT("Fotocamere: {0}   punti: {1}   tracce: {2}"),
    NL("Camera's: {0}   punten: {1}   sporen: {2}"),
    RU("Камер: {0}   точек: {1}   треков: {2}"),
    TR("Kamera: {0}   nokta: {1}   iz: {2}"));

SS_MSG(word_yes, EN("yes"), JA("あり"), ZH_HANS("有"), ZH_HANT("有"), KO("있음"),
    DE("ja"), FR("oui"), ES("sí"), PT("sim"), IT("sì"), NL("ja"), RU("есть"), TR("var"));
SS_MSG(word_no, EN("no"), JA("なし"), ZH_HANS("无"), ZH_HANT("無"), KO("없음"),
    DE("nein"), FR("non"), ES("no"), PT("não"), IT("no"), NL("nee"), RU("нет"), TR("yok"));

SS_MSG(log_covisibility,
    EN("Covisibility from {0}: camera pairs {1}"),
    JA("{0} による共視性: カメラ対 {1}"),
    ZH_HANS("来自 {0} 的共视关系：相机对 {1}"),
    ZH_HANT("來自 {0} 的共視關係：相機對 {1}"),
    KO("{0} 기반 공가시성: 카메라 쌍 {1}"),
    DE("Kovisibilität aus {0}: Kamerapaare {1}"),
    FR("Covisibilité issue de {0} : paires de caméras {1}"),
    ES("Covisibilidad a partir de {0}: pares de cámaras {1}"),
    PT("Covisibilidade a partir de {0}: pares de câmaras {1}"),
    IT("Covisibilità da {0}: coppie di fotocamere {1}"),
    NL("Covisibiliteit uit {0}: cameraparen {1}"),
    RU("Совидимость из {0}: пар камер {1}"),
    TR("{0} kaynaklı ortak görünürlük: kamera çifti {1}"));

SS_MSG(log_summary,
    EN("Parts: {0}   covisibility cut across parts: {1}%   ownership field seeds: {2}"),
    JA("パート: {0}   パート間で切れた共視性: {1}%   所有領域フィールドの点: {2}"),
    ZH_HANS("分区：{0}   被切断的共视关系：{1}%   归属场种子点：{2}"),
    ZH_HANT("分區：{0}   被切斷的共視關係：{1}%   歸屬場種子點：{2}"),
    KO("파트: {0}   파트 사이에서 잘린 공가시성: {1}%   소유 필드 시드: {2}"),
    DE("Teile: {0}   zwischen Teilen durchtrennte Kovisibilität: {1}%   Startpunkte des Zugehörigkeitsfelds: {2}"),
    FR("Parties : {0}   covisibilité coupée entre parties : {1}%   points du champ d'appartenance : {2}"),
    ES("Partes: {0}   covisibilidad cortada entre partes: {1}%   semillas del campo de pertenencia: {2}"),
    PT("Partes: {0}   covisibilidade cortada entre partes: {1}%   sementes do campo de pertença: {2}"),
    IT("Parti: {0}   covisibilità tagliata tra le parti: {1}%   semi del campo di appartenenza: {2}"),
    NL("Delen: {0}   covisibiliteit doorgesneden tussen delen: {1}%   zaden van het eigendomsveld: {2}"),
    RU("Частей: {0}   совидимость, разрезанная между частями: {1}%   точек поля принадлежности: {2}"),
    TR("Parça: {0}   parçalar arasında kesilen ortak görünürlük: %{1}   aidiyet alanı tohumu: {2}"));

SS_MSG(log_part,
    EN("  part {0}: core {1}   ring {2}   seed points {3}   its cameras see {4}% of it"),
    JA("  パート {0}: コア {1}   リング {2}   初期点 {3}   カメラの視野のうち自パート {4}%"),
    ZH_HANS("  分区 {0}：核心 {1}   外环 {2}   种子点 {3}   相机视野中本分区占 {4}%"),
    ZH_HANT("  分區 {0}：核心 {1}   外環 {2}   種子點 {3}   相機視野中本分區佔 {4}%"),
    KO("  파트 {0}: 핵심 {1}   고리 {2}   시드 점 {3}   카메라 시야 중 자기 파트 {4}%"),
    DE("  Teil {0}: Kern {1}   Ring {2}   Startpunkte {3}   eigener Anteil der Sicht {4}%"),
    FR("  partie {0} : cœur {1}   anneau {2}   points d'amorce {3}   part propre de la vue {4} %"),
    ES("  parte {0}: núcleo {1}   anillo {2}   puntos semilla {3}   parte propia de la vista {4}%"),
    PT("  parte {0}: núcleo {1}   anel {2}   pontos semente {3}   parte própria da vista {4}%"),
    IT("  parte {0}: nucleo {1}   anello {2}   punti seme {3}   quota propria della vista {4}%"),
    NL("  deel {0}: kern {1}   ring {2}   zaadpunten {3}   eigen deel van het zicht {4}%"),
    RU("  часть {0}: ядро {1}   кольцо {2}   начальных точек {3}   своя доля обзора {4}%"),
    TR("  parça {0}: çekirdek {1}   halka {2}   tohum noktası {3}   görüşün kendi payı %{4}"));

SS_MSG(log_part_pieces,
    EN("  part {0} is not one piece of the view graph but {1}"),
    JA("  パート {0} はビューグラフ上でひとつながりではなく {1} 個に分かれています"),
    ZH_HANS("  分区 {0} 在视图图中不是一块，而是 {1} 块"),
    ZH_HANT("  分區 {0} 在視圖圖中不是一塊，而是 {1} 塊"),
    KO("  파트 {0}은(는) 뷰 그래프에서 한 덩어리가 아니라 {1} 덩어리입니다"),
    DE("  Teil {0} ist im Sichtgraphen kein zusammenhängendes Stück, sondern {1}"),
    FR("  la partie {0} n'est pas d'un seul tenant dans le graphe de vues mais {1}"),
    ES("  la parte {0} no es una sola pieza del grafo de vistas sino {1}"),
    PT("  a parte {0} não é uma só peça do grafo de vistas mas {1}"),
    IT("  la parte {0} non è un solo pezzo del grafo delle viste ma {1}"),
    NL("  deel {0} is geen enkel stuk van de zichtgraaf maar {1}"),
    RU("  часть {0} в графе видов состоит не из одного куска, а из {1}"),
    TR("  parça {0} görüş grafında tek parça değil, {1} parça"));

SS_MSG(log_written,
    EN("Partition written: {0}"), JA("分割を書き出しました: {0}"),
    ZH_HANS("已写出分区：{0}"), ZH_HANT("已寫出分區：{0}"), KO("분할을 기록했습니다: {0}"),
    DE("Partition geschrieben: {0}"), FR("Partition écrite : {0}"),
    ES("Partición escrita: {0}"), PT("Partição escrita: {0}"),
    IT("Partizione scritta: {0}"), NL("Partitie geschreven: {0}"),
    RU("Разбиение записано: {0}"), TR("Bölümleme yazıldı: {0}"));

SS_MSG(log_next,
    EN("Train part k with: {0} train {1} --partition {2} --partition-part k"),
    JA("パート k の学習: {0} train {1} --partition {2} --partition-part k"),
    ZH_HANS("训练分区 k：{0} train {1} --partition {2} --partition-part k"),
    ZH_HANT("訓練分區 k：{0} train {1} --partition {2} --partition-part k"),
    KO("파트 k 학습: {0} train {1} --partition {2} --partition-part k"),
    DE("Teil k trainieren mit: {0} train {1} --partition {2} --partition-part k"),
    FR("Entraîner la partie k : {0} train {1} --partition {2} --partition-part k"),
    ES("Entrenar la parte k: {0} train {1} --partition {2} --partition-part k"),
    PT("Treinar a parte k: {0} train {1} --partition {2} --partition-part k"),
    IT("Addestrare la parte k: {0} train {1} --partition {2} --partition-part k"),
    NL("Deel k trainen met: {0} train {1} --partition {2} --partition-part k"),
    RU("Обучить часть k: {0} train {1} --partition {2} --partition-part k"),
    TR("k parçasını eğitmek için: {0} train {1} --partition {2} --partition-part k"));

SS_MSG(merge_part_missing,
    EN("  part {0}: no trained model found"),
    JA("  パート {0}: 学習済みモデルが見つかりません"),
    ZH_HANS("  分区 {0}：没有找到训练好的模型"),
    ZH_HANT("  分區 {0}：沒有找到訓練好的模型"),
    KO("  파트 {0}: 학습된 모델이 없습니다"),
    DE("  Teil {0}: kein trainiertes Modell gefunden"),
    FR("  partie {0} : aucun modèle entraîné trouvé"),
    ES("  parte {0}: no se encontró ningún modelo entrenado"),
    PT("  parte {0}: nenhum modelo treinado encontrado"),
    IT("  parte {0}: nessun modello addestrato trovato"),
    NL("  deel {0}: geen getraind model gevonden"),
    RU("  часть {0}: обученная модель не найдена"),
    TR("  parça {0}: eğitilmiş model bulunamadı"));

SS_MSG(merge_part,
    EN("  part {0}: splats {1}   inside its region {2}   from {3}"),
    JA("  パート {0}: スプラット {1}   自領域内 {2}   元 {3}"),
    ZH_HANS("  分区 {0}：泼溅 {1}   位于自身区域内 {2}   来自 {3}"),
    ZH_HANT("  分區 {0}：潑濺 {1}   位於自身區域內 {2}   來自 {3}"),
    KO("  파트 {0}: 스플랫 {1}   자기 영역 안 {2}   출처 {3}"),
    DE("  Teil {0}: Splats {1}   im eigenen Bereich {2}   aus {3}"),
    FR("  partie {0} : splats {1}   dans sa région {2}   depuis {3}"),
    ES("  parte {0}: splats {1}   dentro de su región {2}   de {3}"),
    PT("  parte {0}: splats {1}   dentro da sua região {2}   de {3}"),
    IT("  parte {0}: splat {1}   dentro la sua regione {2}   da {3}"),
    NL("  deel {0}: splats {1}   binnen zijn gebied {2}   uit {3}"),
    RU("  часть {0}: сплатов {1}   внутри своей области {2}   из {3}"),
    TR("  parça {0}: splat {1}   kendi bölgesinde {2}   kaynak {3}"));

SS_MSG(merge_written,
    EN("Merged model written: {0}   splats {1}"),
    JA("結合したモデルを書き出しました: {0}   スプラット {1}"),
    ZH_HANS("已写出合并模型：{0}   泼溅 {1}"),
    ZH_HANT("已寫出合併模型：{0}   潑濺 {1}"),
    KO("병합 모델을 기록했습니다: {0}   스플랫 {1}"),
    DE("Zusammengeführtes Modell geschrieben: {0}   Splats {1}"),
    FR("Modèle fusionné écrit : {0}   splats {1}"),
    ES("Modelo fusionado escrito: {0}   splats {1}"),
    PT("Modelo fundido escrito: {0}   splats {1}"),
    IT("Modello unito scritto: {0}   splat {1}"),
    NL("Samengevoegd model geschreven: {0}   splats {1}"),
    RU("Объединённая модель записана: {0}   сплатов {1}"),
    TR("Birleştirilmiş model yazıldı: {0}   splat {1}"));

SS_MSG(err_nothing_to_merge,
    EN("No part has a trained model; nothing to merge."),
    JA("学習済みモデルのあるパートがなく、結合するものがありません。"),
    ZH_HANS("没有任何分区有训练好的模型，无可合并。"),
    ZH_HANT("沒有任何分區有訓練好的模型，無可合併。"),
    KO("학습된 모델이 있는 파트가 없어 병합할 것이 없습니다."),
    DE("Kein Teil hat ein trainiertes Modell; nichts zusammenzuführen."),
    FR("Aucune partie n'a de modèle entraîné ; rien à fusionner."),
    ES("Ninguna parte tiene un modelo entrenado; nada que fusionar."),
    PT("Nenhuma parte tem um modelo treinado; nada a fundir."),
    IT("Nessuna parte ha un modello addestrato; niente da unire."),
    NL("Geen deel heeft een getraind model; niets samen te voegen."),
    RU("Ни у одной части нет обученной модели; объединять нечего."),
    TR("Hiçbir parçanın eğitilmiş modeli yok; birleştirilecek bir şey yok."));

// ===========================================================================
// The GUI panel
// ===========================================================================

SS_MSG(panel_title,
    EN("Partition"), JA("分割"), ZH_HANS("分区"), ZH_HANT("分區"), KO("분할"),
    DE("Partition"), FR("Partition"), ES("Partición"), PT("Partição"),
    IT("Partizione"), NL("Partitie"), RU("Разбиение"), TR("Bölümleme"));

SS_MSG(chip_part,
    EN("part {0}"), JA("パート {0}"), ZH_HANS("分区 {0}"), ZH_HANT("分區 {0}"),
    KO("파트 {0}"), DE("Teil {0}"), FR("partie {0}"), ES("parte {0}"), PT("parte {0}"),
    IT("parte {0}"), NL("deel {0}"), RU("часть {0}"), TR("parça {0}"));

SS_MSG(open_button,
    EN("Partition"), JA("分割"), ZH_HANS("分区"), ZH_HANT("分區"), KO("분할"),
    DE("Partitionieren"), FR("Partitionner"), ES("Particionar"), PT("Particionar"),
    IT("Partiziona"), NL("Partitioneren"), RU("Разбить"), TR("Bölümle"));

SS_MSG(open_button_help,
    EN("Split this reconstruction into parts that train one at a time on this "
       "GPU, queue them in Batch, and merge the models back into one when they "
       "are done."),
    JA("この再構成を、この GPU で 1 つずつ学習するパートに分割し、バッチに並べ、"
       "終わったらモデルをひとつに結合します。"),
    ZH_HANS("把这次重建拆成可在本 GPU 上逐个训练的分区，排进批处理，训练完后再把模型"
            "合并成一个。"),
    ZH_HANT("把這次重建拆成可在本 GPU 上逐個訓練的分區，排進批次處理，訓練完後再把模型"
            "合併成一個。"),
    KO("이 재구성을 이 GPU에서 하나씩 학습할 파트로 나누고, 배치에 넣고, 끝나면 "
       "모델을 다시 하나로 병합합니다."),
    DE("Diese Rekonstruktion in Teile zerlegen, die auf dieser GPU nacheinander "
       "trainieren, sie in den Stapel stellen und die Modelle danach wieder zu "
       "einem zusammenführen."),
    FR("Découper cette reconstruction en parties entraînées une à une sur ce GPU, "
       "les mettre en file dans Lot, puis refusionner les modèles en un seul."),
    ES("Dividir esta reconstrucción en partes que se entrenan una a una en esta "
       "GPU, ponerlas en cola en Lote y fusionar los modelos en uno al terminar."),
    PT("Dividir esta reconstrução em partes treinadas uma a uma nesta GPU, "
       "pô-las em fila no Lote e voltar a fundir os modelos num só no fim."),
    IT("Dividere questa ricostruzione in parti addestrate una alla volta su questa "
       "GPU, metterle in coda nel Lotto e riunire i modelli in uno alla fine."),
    NL("Deze reconstructie splitsen in delen die één voor één op deze GPU trainen, "
       "ze in Batch zetten en de modellen daarna weer tot één samenvoegen."),
    RU("Разбить эту реконструкцию на части, обучаемые по одной на этой GPU, "
       "поставить их в пакет и потом объединить модели в одну."),
    TR("Bu yeniden oluşturmayı bu GPU'da tek tek eğitilecek parçalara bölmek, "
       "Toplu İşe sıralamak ve bitince modelleri yeniden tek modelde birleştirmek."));

SS_MSG(status_reading,
    EN("Reading the reconstruction..."), JA("再構成を読み込んでいます…"),
    ZH_HANS("正在读取重建…"), ZH_HANT("正在讀取重建…"), KO("재구성을 읽는 중..."),
    DE("Rekonstruktion wird gelesen ..."), FR("Lecture de la reconstruction..."),
    ES("Leyendo la reconstrucción..."), PT("A ler a reconstrução..."),
    IT("Lettura della ricostruzione..."), NL("Reconstructie wordt gelezen..."),
    RU("Чтение реконструкции..."), TR("Yeniden oluşturma okunuyor..."));

SS_MSG(status_computing,
    EN("Computing the partition..."), JA("分割を計算しています…"),
    ZH_HANS("正在计算分区…"), ZH_HANT("正在計算分區…"), KO("분할을 계산하는 중..."),
    DE("Partition wird berechnet ..."), FR("Calcul de la partition..."),
    ES("Calculando la partición..."), PT("A calcular a partição..."),
    IT("Calcolo della partizione..."), NL("Partitie wordt berekend..."),
    RU("Вычисление разбиения..."), TR("Bölümleme hesaplanıyor..."));

SS_MSG(status_cancelled,
    EN("Cancelled; the partition shown is the previous one, if any."),
    JA("中止しました。表示中の分割は前回のもの（あれば）です。"),
    ZH_HANS("已取消；显示的是之前的分区（如果有）。"),
    ZH_HANT("已取消；顯示的是先前的分區（如果有）。"),
    KO("취소했습니다. 보이는 분할은 이전 것입니다(있다면)."),
    DE("Abgebrochen; angezeigt wird die vorige Partition, falls es eine gibt."),
    FR("Annulé ; la partition affichée est la précédente, s'il y en a une."),
    ES("Cancelado; la partición mostrada es la anterior, si la hay."),
    PT("Cancelado; a partição mostrada é a anterior, se houver."),
    IT("Annullato; la partizione mostrata è quella precedente, se c'è."),
    NL("Geannuleerd; de getoonde partitie is de vorige, als die er is."),
    RU("Отменено; показано предыдущее разбиение, если оно было."),
    TR("İptal edildi; gösterilen bölümleme, varsa, öncekidir."));

SS_MSG(warn_no_tracks,
    EN("This dataset has no feature tracks (which camera saw which point), so the "
       "split guesses visibility by projecting the points into the frames. A COLMAP "
       "reconstruction of the same capture (sparse/0 with images.bin and points3D.bin) "
       "has them and gives much better parts."),
    JA("このデータセットには特徴トラック（どのカメラがどの点を見たか）がないため、"
       "点をフレームに投影して可視性を推測します。同じ撮影の COLMAP 再構成"
       "（images.bin と points3D.bin を含む sparse/0）にはトラックがあり、"
       "はるかに良い分割になります。"),
    ZH_HANS("此数据集没有特征轨迹（哪台相机看到了哪个点），因此分区只能把点投影到各帧中"
            "来猜测可见性。同一拍摄的 COLMAP 重建（含 images.bin 和 points3D.bin 的 "
            "sparse/0）带有轨迹，分区效果会好得多。"),
    ZH_HANT("此資料集沒有特徵軌跡（哪台相機看到了哪個點），因此分區只能把點投影到各幀中"
            "來猜測可見性。同一拍攝的 COLMAP 重建（含 images.bin 和 points3D.bin 的 "
            "sparse/0）帶有軌跡，分區效果會好得多。"),
    KO("이 데이터셋에는 특징 트랙(어느 카메라가 어느 점을 봤는지)이 없어서, 점을 "
       "프레임에 투영해 가시성을 추측합니다. 같은 촬영의 COLMAP 재구성(images.bin과 "
       "points3D.bin이 있는 sparse/0)에는 트랙이 있어 훨씬 나은 분할을 얻습니다."),
    DE("Dieser Datensatz hat keine Feature-Spuren (welche Kamera welchen Punkt sah), "
       "daher schätzt die Teilung die Sichtbarkeit, indem sie die Punkte in die Bilder "
       "projiziert. Eine COLMAP-Rekonstruktion derselben Aufnahme (sparse/0 mit "
       "images.bin und points3D.bin) hat sie und ergibt viel bessere Teile."),
    FR("Ce jeu de données n'a pas de pistes de points (quelle caméra a vu quel point) : "
       "le découpage devine donc la visibilité en projetant les points dans les images. "
       "Une reconstruction COLMAP de la même prise (sparse/0 avec images.bin et "
       "points3D.bin) les contient et donne de bien meilleures parties."),
    ES("Este conjunto de datos no tiene pistas de puntos (qué cámara vio qué punto), "
       "así que la división adivina la visibilidad proyectando los puntos en los "
       "fotogramas. Una reconstrucción de COLMAP de la misma captura (sparse/0 con "
       "images.bin y points3D.bin) las tiene y da partes mucho mejores."),
    PT("Este conjunto de dados não tem pistas de pontos (que câmara viu que ponto), "
       "por isso a divisão adivinha a visibilidade projetando os pontos nas imagens. "
       "Uma reconstrução COLMAP da mesma captura (sparse/0 com images.bin e "
       "points3D.bin) tem-nas e dá partes muito melhores."),
    IT("Questo dataset non ha tracce dei punti (quale fotocamera ha visto quale "
       "punto), quindi la divisione indovina la visibilità proiettando i punti nei "
       "fotogrammi. Una ricostruzione COLMAP della stessa ripresa (sparse/0 con "
       "images.bin e points3D.bin) le ha e dà parti molto migliori."),
    NL("Deze dataset heeft geen puntsporen (welke camera welk punt zag), dus de "
       "splitsing raadt de zichtbaarheid door de punten in de beelden te projecteren. "
       "Een COLMAP-reconstructie van dezelfde opname (sparse/0 met images.bin en "
       "points3D.bin) heeft ze en geeft veel betere delen."),
    RU("В этом наборе данных нет треков (какая камера видела какую точку), поэтому "
       "разбиение угадывает видимость, проецируя точки в кадры. Реконструкция COLMAP "
       "той же съёмки (sparse/0 с images.bin и points3D.bin) содержит их и даёт "
       "гораздо лучшие части."),
    TR("Bu veri kümesinde nokta izleri (hangi kameranın hangi noktayı gördüğü) yok; "
       "bu yüzden bölme, noktaları karelere izdüşürerek görünürlüğü tahmin ediyor. "
       "Aynı çekimin bir COLMAP yeniden oluşturması (images.bin ve points3D.bin içeren "
       "sparse/0) bu izleri içerir ve çok daha iyi parçalar verir."));

SS_MSG(status_summary,
    EN("Parts: {0}   cameras: {1}   points: {2}   covisibility cut: {3}%   from {4}"),
    JA("パート: {0}   カメラ: {1}   点: {2}   切れた共視性: {3}%   出どころ {4}"),
    ZH_HANS("分区：{0}   相机：{1}   点：{2}   被切断的共视关系：{3}%   来源 {4}"),
    ZH_HANT("分區：{0}   相機：{1}   點：{2}   被切斷的共視關係：{3}%   來源 {4}"),
    KO("파트: {0}   카메라: {1}   점: {2}   잘린 공가시성: {3}%   출처 {4}"),
    DE("Teile: {0}   Kameras: {1}   Punkte: {2}   durchtrennte Kovisibilität: {3}%   aus {4}"),
    FR("Parties : {0}   caméras : {1}   points : {2}   covisibilité coupée : {3}%   source {4}"),
    ES("Partes: {0}   cámaras: {1}   puntos: {2}   covisibilidad cortada: {3}%   de {4}"),
    PT("Partes: {0}   câmaras: {1}   pontos: {2}   covisibilidade cortada: {3}%   de {4}"),
    IT("Parti: {0}   fotocamere: {1}   punti: {2}   covisibilità tagliata: {3}%   da {4}"),
    NL("Delen: {0}   camera's: {1}   punten: {2}   doorgesneden covisibiliteit: {3}%   uit {4}"),
    RU("Частей: {0}   камер: {1}   точек: {2}   разрезанная совидимость: {3}%   из {4}"),
    TR("Parça: {0}   kamera: {1}   nokta: {2}   kesilen ortak görünürlük: %{3}   kaynak {4}"));

SS_MSG(lbl_method,
    EN("Split"), JA("分割方法"), ZH_HANS("切分方式"), ZH_HANT("切分方式"), KO("분할 방식"),
    DE("Aufteilung"), FR("Découpe"), ES("División"), PT("Divisão"), IT("Suddivisione"),
    NL("Opsplitsing"), RU("Разбиение"), TR("Bölme"));
SS_MSG(meth_graph,
    EN("Visibility cut"), JA("可視性カット"), ZH_HANS("可见性切分"), ZH_HANT("可見性切分"),
    KO("가시성 컷"), DE("Sichtbarkeitsschnitt"), FR("Coupe de visibilité"),
    ES("Corte de visibilidad"), PT("Corte de visibilidade"), IT("Taglio di visibilità"),
    NL("Zichtbaarheidssnede"), RU("Разрез по видимости"), TR("Görünürlük kesimi"));
SS_MSG(meth_viewgraph,
    EN("Cameras first (view graph)"), JA("カメラを先に（ビューグラフ）"),
    ZH_HANS("先分相机（视图图）"), ZH_HANT("先分相機（視圖圖）"), KO("카메라 먼저(뷰 그래프)"),
    DE("Kameras zuerst (Sichtgraph)"), FR("Caméras d'abord (graphe de vues)"),
    ES("Cámaras primero (grafo de vistas)"), PT("Câmaras primeiro (grafo de vistas)"),
    IT("Prima le fotocamere (grafo delle viste)"), NL("Eerst camera's (zichtgraaf)"),
    RU("Сначала камеры (граф видов)"), TR("Önce kameralar (görüş grafı)"));

SS_MSG(lbl_source,
    EN("Covisibility"), JA("共視性"), ZH_HANS("共视关系"), ZH_HANT("共視關係"),
    KO("공가시성"), DE("Kovisibilität"), FR("Covisibilité"), ES("Covisibilidad"),
    PT("Covisibilidade"), IT("Covisibilità"), NL("Covisibiliteit"), RU("Совидимость"),
    TR("Ortak görünürlük"));

SS_MSG(src_auto,
    EN("Automatic"), JA("自動"), ZH_HANS("自动"), ZH_HANT("自動"), KO("자동"),
    DE("Automatisch"), FR("Automatique"), ES("Automática"), PT("Automática"),
    IT("Automatica"), NL("Automatisch"), RU("Автоматически"), TR("Otomatik"));
SS_MSG(src_tracks,
    EN("Tracks of the model"), JA("モデルのトラック"), ZH_HANS("模型的轨迹"),
    ZH_HANT("模型的軌跡"), KO("모델의 트랙"), DE("Spuren des Modells"),
    FR("Pistes du modèle"), ES("Pistas del modelo"), PT("Pistas do modelo"),
    IT("Tracce del modello"), NL("Sporen van het model"), RU("Треки модели"),
    TR("Modelin izleri"));
SS_MSG(src_projection,
    EN("Seed cloud projected"), JA("初期点群の投影"), ZH_HANS("投影种子点云"),
    ZH_HANT("投影種子點雲"), KO("시드 점 구름 투영"), DE("Projizierte Startpunktwolke"),
    FR("Nuage d'amorce projeté"), ES("Nube semilla proyectada"),
    PT("Nuvem semente projetada"), IT("Nuvola seme proiettata"),
    NL("Geprojecteerde zaadwolk"), RU("Проекция начального облака"),
    TR("Yansıtılan tohum bulutu"));
SS_MSG(src_proximity,
    EN("Camera positions"), JA("カメラ位置"), ZH_HANS("相机位置"), ZH_HANT("相機位置"),
    KO("카메라 위치"), DE("Kamerapositionen"), FR("Positions des caméras"),
    ES("Posiciones de las cámaras"), PT("Posições das câmaras"),
    IT("Posizioni delle fotocamere"), NL("Cameraposities"), RU("Положения камер"),
    TR("Kamera konumları"));

SS_MSG(lbl_source_help,
    EN("What decides which cameras belong together. The model's own tracks are "
       "exact and free; a dataset without them gets the seed cloud projected into "
       "every frame; a dataset with no cloud is cut by where the cameras stand."),
    JA("どのカメラが同じパートに入るかを決めるもの。モデル自身のトラックは正確で追"
       "加コストなし。トラックがないデータセットは初期点群を全フレームに投影し、点"
       "群もなければカメラの位置で切ります。"),
    ZH_HANS("决定哪些相机归为一组的依据。模型自带的轨迹最准且无额外开销；没有轨迹的"
            "数据集把种子点云投影到每一帧；连点云都没有的按相机位置切分。"),
    ZH_HANT("決定哪些相機歸為一組的依據。模型自帶的軌跡最準且無額外開銷；沒有軌跡的"
            "資料集把種子點雲投影到每一幀；連點雲都沒有的按相機位置切分。"),
    KO("어떤 카메라가 함께 묶이는지를 정하는 근거. 모델의 트랙은 정확하고 비용이 "
       "없고, 트랙이 없는 데이터셋은 시드 점 구름을 모든 프레임에 투영하며, 점 구름"
       "도 없으면 카메라 위치로 나눕니다."),
    DE("Was entscheidet, welche Kameras zusammengehören. Die Spuren des Modells "
       "sind exakt und kostenlos; ein Datensatz ohne sie bekommt die "
       "Startpunktwolke in jedes Bild projiziert; einer ohne Wolke wird nach den "
       "Kamerastandorten geschnitten."),
    FR("Ce qui décide quelles caméras vont ensemble. Les pistes du modèle sont "
       "exactes et gratuites ; sans elles, le nuage d'amorce est projeté dans "
       "chaque image ; sans nuage, la coupe suit la position des caméras."),
    ES("Lo que decide qué cámaras van juntas. Las pistas del modelo son exactas y "
       "gratis; sin ellas se proyecta la nube semilla en cada imagen; sin nube, se "
       "corta por la posición de las cámaras."),
    PT("O que decide quais câmaras ficam juntas. As pistas do modelo são exatas e "
       "sem custo; sem elas a nuvem semente é projetada em cada imagem; sem nuvem, "
       "corta-se pela posição das câmaras."),
    IT("Ciò che decide quali fotocamere stanno insieme. Le tracce del modello sono "
       "esatte e gratuite; senza di esse la nuvola seme viene proiettata in ogni "
       "immagine; senza nuvola si taglia per la posizione delle fotocamere."),
    NL("Wat bepaalt welke camera's bij elkaar horen. De sporen van het model zijn "
       "exact en gratis; zonder die wordt de zaadwolk in elk beeld geprojecteerd; "
       "zonder wolk wordt gesneden op waar de camera's staan."),
    RU("Что решает, какие камеры относятся к одной части. Треки модели точны и "
       "бесплатны; без них начальное облако проецируется в каждый кадр; без облака "
       "разрез идёт по положениям камер."),
    TR("Hangi kameraların birlikte olacağını ne belirler. Modelin izleri kesin ve "
       "bedavadır; izi olmayan veri kümesinde tohum bulutu her kareye yansıtılır; "
       "bulutu da olmayan kameraların durduğu yere göre kesilir."));

SS_MSG(lbl_split_by,
    EN("Split by"), JA("分割の基準"), ZH_HANS("拆分依据"), ZH_HANT("拆分依據"),
    KO("나누는 기준"), DE("Teilen nach"), FR("Découper par"), ES("Dividir por"),
    PT("Dividir por"), IT("Dividi per"), NL("Splitsen op"), RU("Делить по"),
    TR("Bölme ölçütü"));
SS_MSG(mode_parts,
    EN("Number of parts"), JA("パート数"), ZH_HANS("分区数量"), ZH_HANT("分區數量"),
    KO("파트 수"), DE("Anzahl Teile"), FR("Nombre de parties"), ES("Número de partes"),
    PT("Número de partes"), IT("Numero di parti"), NL("Aantal delen"),
    RU("Число частей"), TR("Parça sayısı"));
SS_MSG(mode_max,
    EN("Cameras per part"), JA("パートあたりのカメラ数"), ZH_HANS("每个分区的相机数"),
    ZH_HANT("每個分區的相機數"), KO("파트당 카메라 수"), DE("Kameras je Teil"),
    FR("Caméras par partie"), ES("Cámaras por parte"), PT("Câmaras por parte"),
    IT("Fotocamere per parte"), NL("Camera's per deel"), RU("Камер на часть"),
    TR("Parça başına kamera"));
SS_MSG(mode_help,
    EN("Either ask for a count, or for a ceiling on how many cameras a part's "
       "core may hold -- what one training run of this GPU can take."),
    JA("パート数を指定するか、パートのコアに入るカメラ数の上限（この GPU の 1 回の"
       "学習でこなせる量）を指定します。"),
    ZH_HANS("要么指定分区数量，要么指定每个分区核心可容纳的相机上限，即本 GPU 一次"
            "训练能承受的量。"),
    ZH_HANT("要麼指定分區數量，要麼指定每個分區核心可容納的相機上限，即本 GPU 一次"
            "訓練能承受的量。"),
    KO("파트 수를 지정하거나, 파트 핵심에 들어갈 카메라 수의 상한(이 GPU의 한 번 "
       "학습이 감당할 양)을 지정합니다."),
    DE("Entweder eine Anzahl verlangen oder eine Obergrenze, wie viele Kameras der "
       "Kern eines Teils haben darf -- was ein Trainingslauf dieser GPU schafft."),
    FR("Soit un nombre de parties, soit un plafond de caméras dans le cœur d'une "
       "partie -- ce qu'un entraînement sur ce GPU peut absorber."),
    ES("O bien un número de partes, o bien un tope de cámaras en el núcleo de una "
       "parte: lo que una ejecución en esta GPU puede asumir."),
    PT("Ou um número de partes, ou um teto de câmaras no núcleo de uma parte -- o "
       "que um treino nesta GPU consegue aguentar."),
    IT("O un numero di parti, oppure un tetto alle fotocamere nel nucleo di una "
       "parte -- quanto un addestramento su questa GPU può reggere."),
    NL("Vraag om een aantal, of om een plafond voor hoeveel camera's de kern van "
       "een deel mag hebben -- wat één trainingsrun op deze GPU aankan."),
    RU("Либо число частей, либо потолок числа камер в ядре части -- то, что один "
       "запуск обучения на этой GPU способен взять."),
    TR("Ya bir sayı isteyin, ya da bir parçanın çekirdeğindeki kamera sayısına "
       "bir üst sınır -- bu GPU'da tek bir eğitimin kaldırabileceği kadar."));

SS_MSG(lbl_ring,
    EN("Ring share"), JA("リングの割合"), ZH_HANS("外环比例"), ZH_HANT("外環比例"),
    KO("고리 비율"), DE("Ringanteil"), FR("Part de l'anneau"), ES("Umbral del anillo"),
    PT("Limiar do anel"), IT("Soglia dell'anello"), NL("Ringaandeel"), RU("Порог кольца"),
    TR("Halka payı"));
SS_MSG(lbl_ring_help,
    EN("An outside camera trains with a part when at least this share of the "
       "points it sees belongs to the part. Higher means fewer borrowed cameras "
       "and faster parts; lower means seams seen from both sides by more views."),
    JA("外側のカメラは、見ている点のうちこの割合以上がパートに属するときそのパー"
       "トと一緒に学習します。高くすると借りるカメラが減って速く、低くすると継ぎ目"
       "を両側から見る視点が増えます。"),
    ZH_HANS("外围相机所见点中属于该分区的比例达到此值时，与该分区一起训练。值越高借"
            "用的相机越少、训练越快；值越低则有更多视角从两侧覆盖接缝。"),
    ZH_HANT("外圍相機所見點中屬於該分區的比例達到此值時，與該分區一起訓練。值越高借"
            "用的相機越少、訓練越快；值越低則有更多視角從兩側覆蓋接縫。"),
    KO("바깥 카메라는 자신이 보는 점 중 이 비율 이상이 파트에 속할 때 그 파트와 함께 "
       "학습합니다. 높이면 빌리는 카메라가 줄어 빨라지고, 낮추면 더 많은 시점이 이음"
       "새를 양쪽에서 봅니다."),
    DE("Eine äußere Kamera trainiert mit einem Teil, wenn mindestens dieser Anteil "
       "der von ihr gesehenen Punkte zum Teil gehört. Höher heißt weniger "
       "geliehene Kameras und schnellere Teile; niedriger heißt Nähte, die mehr "
       "Ansichten von beiden Seiten sehen."),
    FR("Une caméra extérieure s'entraîne avec une partie quand au moins cette part "
       "des points qu'elle voit lui appartient. Plus haut : moins de caméras "
       "empruntées et des parties plus rapides ; plus bas : des coutures vues des "
       "deux côtés par plus de vues."),
    ES("Una cámara externa entrena con una parte cuando al menos esta fracción de "
       "los puntos que ve pertenece a la parte. Más alto: menos cámaras prestadas "
       "y partes más rápidas; más bajo: costuras vistas desde ambos lados por más "
       "vistas."),
    PT("Uma câmara exterior treina com uma parte quando pelo menos esta fração dos "
       "pontos que vê lhe pertence. Mais alto: menos câmaras emprestadas e partes "
       "mais rápidas; mais baixo: costuras vistas de ambos os lados por mais vistas."),
    IT("Una fotocamera esterna si addestra con una parte quando almeno questa quota "
       "dei punti che vede le appartiene. Più alto: meno fotocamere prese in "
       "prestito e parti più veloci; più basso: cuciture viste da entrambi i lati "
       "da più viste."),
    NL("Een buitencamera traint mee met een deel als minstens dit aandeel van de "
       "punten die ze ziet bij het deel hoort. Hoger: minder geleende camera's en "
       "snellere delen; lager: naden die door meer beelden van beide kanten worden "
       "gezien."),
    RU("Внешняя камера обучается с частью, если хотя бы такая доля видимых ею точек "
       "принадлежит части. Выше -- меньше заимствованных камер и быстрее части; "
       "ниже -- швы видны с обеих сторон большим числом видов."),
    TR("Dış bir kamera, gördüğü noktaların en az bu payı parçaya aitse o parçayla "
       "eğitilir. Yüksek: daha az ödünç kamera ve daha hızlı parçalar; düşük: "
       "dikişleri iki yandan gören daha çok görünüm."));

SS_MSG(lbl_seeds,
    EN("Ownership seeds"), JA("所有領域の点"), ZH_HANS("归属种子点"), ZH_HANT("歸屬種子點"),
    KO("소유 시드"), DE("Zugehörigkeitspunkte"), FR("Points d'appartenance"),
    ES("Semillas de pertenencia"), PT("Sementes de pertença"), IT("Semi di appartenenza"),
    NL("Eigendomszaden"), RU("Точки принадлежности"), TR("Aidiyet tohumları"));
SS_MSG(lbl_seeds_help,
    EN("Every point of space belongs to the nearest of these seeds -- the cloud's "
       "points, each knowing which side it was seen from, and the cameras. More "
       "seeds follow the cloud more closely at the seams and cost the merge and "
       "each refine step a little more."),
    JA("空間の各点は、これらの初期点（見られた側を知っている点群の点とカメラ）のう"
       "ち最も近いものに属します。点が多いほど継ぎ目で点群に忠実になり、結合と各"
       "リファインステップのコストが少し増えます。"),
    ZH_HANS("空间中每一点都归属于这些种子点中最近的一个——点云里知道自己被从哪一侧看"
            "到的点，以及相机。种子越多，接缝处越贴近点云，合并和每次细化的开销略增。"),
    ZH_HANT("空間中每一點都歸屬於這些種子點中最近的一個——點雲裡知道自己被從哪一側看"
            "到的點，以及相機。種子越多，接縫處越貼近點雲，合併和每次細化的開銷略增。"),
    KO("공간의 모든 점은 이 시드 중 가장 가까운 것에 속합니다. 어느 쪽에서 보였는지 "
       "아는 점 구름의 점과 카메라입니다. 시드가 많을수록 이음새에서 점 구름을 더 "
       "가깝게 따르고 병합과 각 정제 단계 비용이 조금 늘어납니다."),
    DE("Jeder Punkt des Raums gehört zum nächsten dieser Punkte -- den Punkten "
       "der Wolke, die wissen, von welcher Seite sie gesehen wurden, und den "
       "Kameras. Mehr Punkte folgen der Wolke an den Nähten enger und kosten die "
       "Zusammenführung und jeden Verfeinerungsschritt etwas mehr."),
    FR("Chaque point de l'espace appartient au plus proche de ces points : ceux "
       "du nuage, qui savent de quel côté ils ont été vus, et les caméras. Plus "
       "de points suivent le nuage de plus près aux coutures et coûtent un peu "
       "plus à la fusion et à chaque raffinement."),
    ES("Cada punto del espacio pertenece a la más cercana de estas semillas: los "
       "puntos de la nube, que saben desde qué lado se vieron, y las cámaras. Más "
       "semillas siguen la nube más de cerca en las costuras y encarecen un poco "
       "la fusión y cada paso de refinado."),
    PT("Cada ponto do espaço pertence à mais próxima destas sementes: os pontos "
       "da nuvem, que sabem de que lado foram vistos, e as câmaras. Mais sementes "
       "seguem a nuvem mais de perto nas costuras e custam um pouco mais à fusão "
       "e a cada passo de refinação."),
    IT("Ogni punto dello spazio appartiene al più vicino di questi semi: i punti "
       "della nuvola, che sanno da che lato sono stati visti, e le fotocamere. "
       "Più semi seguono la nuvola più da vicino alle cuciture e costano un po' "
       "di più all'unione e a ogni passo di raffinamento."),
    NL("Elk punt in de ruimte hoort bij het dichtstbijzijnde van deze zaden: de "
       "punten van de wolk, die weten van welke kant ze gezien zijn, en de "
       "camera's. Meer zaden volgen de wolk nauwer bij de naden en kosten het "
       "samenvoegen en elke verfijningsstap iets meer."),
    RU("Каждая точка пространства принадлежит ближайшей из этих точек -- точкам "
       "облака, знающим, с какой стороны их видели, и камерам. Больше точек -- "
       "точнее следование облаку на швах и чуть дороже объединение и каждый шаг "
       "уточнения."),
    TR("Uzaydaki her nokta bu tohumlardan en yakınına aittir: hangi yandan "
       "görüldüğünü bilen bulut noktaları ve kameralar. Daha çok tohum dikişlerde "
       "bulutu daha yakından izler; birleştirme ve her arıtma adımı biraz daha "
       "pahalıdır."));

SS_MSG(btn_compute,
    EN("Compute"), JA("計算"), ZH_HANS("计算"), ZH_HANT("計算"), KO("계산"),
    DE("Berechnen"), FR("Calculer"), ES("Calcular"), PT("Calcular"), IT("Calcola"),
    NL("Berekenen"), RU("Вычислить"), TR("Hesapla"));

SS_MSG(col_part, EN("Part"), JA("パート"), ZH_HANS("分区"), ZH_HANT("分區"), KO("파트"),
    DE("Teil"), FR("Partie"), ES("Parte"), PT("Parte"), IT("Parte"), NL("Deel"),
    RU("Часть"), TR("Parça"));
SS_MSG(col_core, EN("Core"), JA("コア"), ZH_HANS("核心"), ZH_HANT("核心"), KO("핵심"),
    DE("Kern"), FR("Cœur"), ES("Núcleo"), PT("Núcleo"), IT("Nucleo"), NL("Kern"),
    RU("Ядро"), TR("Çekirdek"));
SS_MSG(col_ring, EN("Ring"), JA("リング"), ZH_HANS("外环"), ZH_HANT("外環"), KO("고리"),
    DE("Ring"), FR("Anneau"), ES("Anillo"), PT("Anel"), IT("Anello"), NL("Ring"),
    RU("Кольцо"), TR("Halka"));
SS_MSG(col_points, EN("Points"), JA("点"), ZH_HANS("点"), ZH_HANT("點"), KO("점"),
    DE("Punkte"), FR("Points"), ES("Puntos"), PT("Pontos"), IT("Punti"), NL("Punten"),
    RU("Точки"), TR("Nokta"));
SS_MSG(col_run, EN("Trained model"), JA("学習済みモデル"), ZH_HANS("已训练模型"),
    ZH_HANT("已訓練模型"), KO("학습된 모델"), DE("Trainiertes Modell"),
    FR("Modèle entraîné"), ES("Modelo entrenado"), PT("Modelo treinado"),
    IT("Modello addestrato"), NL("Getraind model"), RU("Обученная модель"),
    TR("Eğitilmiş model"));

SS_MSG(show_all_parts,
    EN("All parts"), JA("すべてのパート"), ZH_HANS("所有分区"), ZH_HANT("所有分區"),
    KO("모든 파트"), DE("Alle Teile"), FR("Toutes les parties"), ES("Todas las partes"),
    PT("Todas as partes"), IT("Tutte le parti"), NL("Alle delen"), RU("Все части"),
    TR("Tüm parçalar"));
SS_MSG(show_help,
    EN("Pick a part to see only what it trains on: its own cameras drawn solid, "
       "its ring drawn faint, the points it seeds from, and the grid cells it "
       "owns."),
    JA("パートを選ぶと、そのパートが学習に使うものだけを表示します。自分のカメラ"
       "は実線、リングは薄く、初期点、所有するグリッドセルです。"),
    ZH_HANS("选择一个分区，只显示它训练所用的内容：自身相机为实线、外环为淡色、它的"
            "种子点，以及它拥有的网格单元。"),
    ZH_HANT("選擇一個分區，只顯示它訓練所用的內容：自身相機為實線、外環為淡色、它的"
            "種子點，以及它擁有的網格單元。"),
    KO("파트를 고르면 그 파트가 학습에 쓰는 것만 보입니다. 자기 카메라는 진하게, "
       "고리는 흐리게, 시드 점, 그리고 소유한 격자 셀입니다."),
    DE("Einen Teil wählen, um nur zu sehen, womit er trainiert: eigene Kameras "
       "kräftig, sein Ring blass, seine Startpunkte und die Gitterzellen, die ihm "
       "gehören."),
    FR("Choisir une partie pour ne voir que ce sur quoi elle s'entraîne : ses "
       "caméras en trait plein, son anneau en pâle, ses points d'amorce et les "
       "cellules qu'elle possède."),
    ES("Elige una parte para ver solo con lo que entrena: sus cámaras en sólido, su "
       "anillo tenue, sus puntos semilla y las celdas que posee."),
    PT("Escolha uma parte para ver só aquilo com que treina: as suas câmaras a "
       "cheio, o seu anel esbatido, os seus pontos semente e as células que possui."),
    IT("Scegli una parte per vedere solo ciò con cui si addestra: le sue fotocamere "
       "piene, il suo anello tenue, i suoi punti seme e le celle che possiede."),
    NL("Kies een deel om alleen te zien waarmee het traint: eigen camera's vol, "
       "zijn ring vaag, zijn zaadpunten en de rastercellen die het bezit."),
    RU("Выберите часть, чтобы видеть только то, на чём она обучается: свои камеры "
       "ярко, кольцо бледно, свои начальные точки и ячейки сетки, которыми владеет."),
    TR("Yalnız neyle eğitildiğini görmek için bir parça seçin: kendi kameraları "
       "dolu, halkası soluk, tohum noktaları ve sahip olduğu ızgara hücreleri."));

SS_MSG(show_owner_colors,
    EN("Colour points by owner"), JA("点を所有パートの色で"), ZH_HANS("按归属给点着色"),
    ZH_HANT("按歸屬給點著色"), KO("점을 소유 파트 색으로"), DE("Punkte nach Besitzer färben"),
    FR("Colorer les points par partie"), ES("Colorear puntos por parte"),
    PT("Colorir pontos por parte"), IT("Colora i punti per parte"),
    NL("Punten kleuren per deel"), RU("Красить точки по части"),
    TR("Noktaları sahibine göre renklendir"));
SS_MSG(show_grid,
    EN("Show regions"), JA("領域を表示"), ZH_HANS("显示区域"), ZH_HANT("顯示區域"),
    KO("영역 표시"), DE("Bereiche zeigen"), FR("Afficher les régions"),
    ES("Mostrar regiones"), PT("Mostrar regiões"), IT("Mostra regioni"),
    NL("Gebieden tonen"), RU("Показать области"), TR("Bölgeleri göster"));
SS_MSG(show_grid_help,
    EN("The surface around the space each part owns, tinted in its colour with a "
       "dashed outline: the seams where the merge will switch from one model to "
       "the next, through empty space as well."),
    JA("各パートが所有する空間を囲む面を、そのパートの色と破線の輪郭で表示します。"
       "マージがモデルを切り替える継ぎ目が、何もない空間でも見えます。"),
    ZH_HANS("每个部分所拥有空间的边界面，以其颜色着色并带虚线轮廓：即合并时从一个模型切换到另一个模型的接缝，空旷处也可见。"),
    ZH_HANT("每個部分所擁有空間的邊界面，以其顏色著色並帶虛線輪廓：即合併時從一個模型切換到另一個模型的接縫，空曠處也可見。"),
    KO("각 파트가 소유한 공간을 둘러싼 면을 그 파트의 색과 점선 윤곽으로 표시합니다. "
       "병합이 한 모델에서 다음 모델로 넘어가는 이음새가 빈 공간에서도 보입니다."),
    DE("Die Fläche um den Raum, der jedem Teil gehört, in seiner Farbe getönt und "
       "gestrichelt umrandet: die Nähte, an denen das Zusammenführen von einem "
       "Modell zum nächsten wechselt, auch durch leeren Raum."),
    FR("La surface autour de l'espace de chaque partie, teintée de sa couleur avec "
       "un contour pointillé : les coutures où la fusion passe d'un modèle au "
       "suivant, y compris dans le vide."),
    ES("La superficie que rodea el espacio de cada parte, tintada de su color con "
       "contorno discontinuo: las costuras donde la fusión pasa de un modelo al "
       "siguiente, también en el espacio vacío."),
    PT("A superfície em torno do espaço de cada parte, tingida da sua cor com "
       "contorno tracejado: as costuras onde a fusão passa de um modelo ao "
       "seguinte, também no espaço vazio."),
    IT("La superficie attorno allo spazio di ogni parte, colorata col suo colore e "
       "con contorno tratteggiato: le cuciture dove l'unione passa da un modello al "
       "successivo, anche nello spazio vuoto."),
    NL("Het oppervlak rond de ruimte van elk deel, getint in zijn kleur met een "
       "gestippelde omtrek: de naden waar het samenvoegen van het ene model naar "
       "het volgende overgaat, ook door lege ruimte."),
    RU("Поверхность вокруг пространства каждой части, в её цвете с пунктирным "
       "контуром: швы, где слияние переходит от одной модели к другой, в том числе "
       "в пустом пространстве."),
    TR("Her parçanın sahip olduğu alanı saran yüzey, kendi renginde ve kesikli bir "
       "çerçeveyle: birleştirmenin bir modelden diğerine geçtiği dikişler, boş "
       "alanda da."));

SS_MSG(lbl_file, EN("File"), JA("ファイル"), ZH_HANS("文件"), ZH_HANT("檔案"), KO("파일"),
    DE("Datei"), FR("Fichier"), ES("Archivo"), PT("Ficheiro"), IT("File"), NL("Bestand"),
    RU("Файл"), TR("Dosya"));
SS_MSG(btn_save,
    EN("Save partition"), JA("分割を保存"), ZH_HANS("保存分区"), ZH_HANT("儲存分區"),
    KO("분할 저장"), DE("Partition speichern"), FR("Enregistrer la partition"),
    ES("Guardar partición"), PT("Guardar partição"), IT("Salva partizione"),
    NL("Partitie opslaan"), RU("Сохранить разбиение"), TR("Bölümlemeyi kaydet"));
SS_MSG(saved_to,
    EN("Saved: {0}"), JA("保存しました: {0}"), ZH_HANS("已保存：{0}"), ZH_HANT("已儲存：{0}"),
    KO("저장했습니다: {0}"), DE("Gespeichert: {0}"), FR("Enregistré : {0}"),
    ES("Guardado: {0}"), PT("Guardado: {0}"), IT("Salvato: {0}"), NL("Opgeslagen: {0}"),
    RU("Сохранено: {0}"), TR("Kaydedildi: {0}"));

SS_MSG(btn_batch,
    EN("Queue parts in Batch"), JA("パートをバッチに追加"), ZH_HANS("把分区排入批处理"),
    ZH_HANT("把分區排入批次處理"), KO("파트를 배치에 넣기"), DE("Teile in den Stapel stellen"),
    FR("Mettre les parties en file dans Lot"), ES("Poner las partes en cola en Lote"),
    PT("Pôr as partes em fila no Lote"), IT("Metti le parti in coda nel Lotto"),
    NL("Delen in Batch zetten"), RU("Поставить части в пакет"),
    TR("Parçaları Toplu İşe sırala"));
SS_MSG(btn_batch_help,
    EN("Saves the partition, then queues one training run per part on the Batch "
       "screen -- this dataset with the run's part set -- using the training "
       "preset shown there. Merge here once they have all trained."),
    JA("分割を保存し、バッチ画面にパートごとの学習を 1 つずつ並べます。このデータ"
       "セットに各パートを指定し、バッチ画面の学習プリセットを使います。全部の学習"
       "が終わったらここで結合します。"),
    ZH_HANS("保存分区，然后在批处理页面为每个分区排一次训练：用本数据集并指定该分区，"
            "训练预设取批处理页面所示。全部训练完后回到这里合并。"),
    ZH_HANT("儲存分區，然後在批次處理頁面為每個分區排一次訓練：用本資料集並指定該分區，"
            "訓練預設取批次處理頁面所示。全部訓練完後回到這裡合併。"),
    KO("분할을 저장한 뒤 배치 화면에 파트마다 학습 하나씩을 넣습니다. 이 데이터셋에 "
       "해당 파트를 지정하고, 배치 화면의 학습 프리셋을 씁니다. 모두 끝나면 여기서 "
       "병합합니다."),
    DE("Speichert die Partition und stellt dann pro Teil einen Trainingslauf in "
       "den Stapel -- dieser Datensatz mit dem jeweiligen Teil -- mit dem dort "
       "gezeigten Trainingspreset. Hier zusammenführen, wenn alle trainiert sind."),
    FR("Enregistre la partition, puis met en file un entraînement par partie sur "
       "l'écran Lot -- ce jeu de données avec la partie du run -- avec le préréglage "
       "d'entraînement affiché là. Fusionnez ici quand tout est entraîné."),
    ES("Guarda la partición y pone en cola una ejecución por parte en la pantalla "
       "Lote (este conjunto de datos con la parte de cada ejecución), con el ajuste "
       "de entrenamiento que allí se muestra. Fusiona aquí cuando todas hayan "
       "terminado."),
    PT("Guarda a partição e põe em fila um treino por parte no ecrã Lote -- este "
       "conjunto de dados com a parte de cada treino -- com a predefinição de "
       "treino aí mostrada. Funda aqui quando todos tiverem terminado."),
    IT("Salva la partizione e mette in coda un addestramento per parte nella "
       "schermata Lotto -- questo insieme di dati con la parte di ogni esecuzione "
       "-- con il preset di addestramento mostrato lì. Unisci qui quando hanno "
       "tutte finito."),
    NL("Slaat de partitie op en zet dan per deel één trainingsrun op het "
       "Batch-scherm -- deze dataset met het deel van de run -- met het daar "
       "getoonde trainingspreset. Voeg hier samen als alles getraind is."),
    RU("Сохраняет разбиение и ставит по одному запуску обучения на часть на экране "
       "пакета -- этот набор с указанной частью -- с показанным там пресетом. "
       "Объединяйте здесь, когда все обучены."),
    TR("Bölümlemeyi kaydeder, sonra Toplu İş ekranına parça başına bir eğitim "
       "çalıştırması sıralar -- bu veri kümesi, çalıştırmanın parçası ayarlı -- "
       "oradaki eğitim ön ayarıyla. Hepsi eğitilince burada birleştirin."));
SS_MSG(batch_added,
    EN("Runs queued: {0}. Start them on the Batch screen."),
    JA("学習を {0} 件並べました。バッチ画面で開始してください。"),
    ZH_HANS("已排入 {0} 次训练。请到批处理页面开始。"),
    ZH_HANT("已排入 {0} 次訓練。請到批次處理頁面開始。"),
    KO("학습 {0}개를 넣었습니다. 배치 화면에서 시작하세요."),
    DE("Läufe eingereiht: {0}. Auf dem Stapel-Bildschirm starten."),
    FR("Entraînements en file : {0}. Lancez-les sur l'écran Lot."),
    ES("Ejecuciones en cola: {0}. Inícialas en la pantalla Lote."),
    PT("Treinos em fila: {0}. Inicie-os no ecrã Lote."),
    IT("Esecuzioni in coda: {0}. Avviale nella schermata Lotto."),
    NL("Runs in de wachtrij: {0}. Start ze op het Batch-scherm."),
    RU("Запусков в очереди: {0}. Запустите их на экране пакета."),
    TR("Sıralanan çalıştırma: {0}. Toplu İş ekranından başlatın."));

SS_MSG(merge_head,
    EN("Merge"), JA("結合"), ZH_HANS("合并"), ZH_HANT("合併"), KO("병합"),
    DE("Zusammenführen"), FR("Fusion"), ES("Fusión"), PT("Fusão"), IT("Unione"),
    NL("Samenvoegen"), RU("Объединение"), TR("Birleştirme"));
SS_MSG(lbl_runs,
    EN("Runs folder"), JA("学習出力フォルダ"), ZH_HANS("训练输出文件夹"),
    ZH_HANT("訓練輸出資料夾"), KO("학습 출력 폴더"), DE("Ordner der Läufe"),
    FR("Dossier des entraînements"), ES("Carpeta de ejecuciones"),
    PT("Pasta dos treinos"), IT("Cartella delle esecuzioni"), NL("Map met runs"),
    RU("Папка запусков"), TR("Çalıştırma klasörü"));
SS_MSG(btn_scan,
    EN("Find models"), JA("モデルを探す"), ZH_HANS("查找模型"), ZH_HANT("尋找模型"),
    KO("모델 찾기"), DE("Modelle suchen"), FR("Chercher les modèles"),
    ES("Buscar modelos"), PT("Procurar modelos"), IT("Trova modelli"),
    NL("Modellen zoeken"), RU("Найти модели"), TR("Modelleri bul"));
SS_MSG(btn_scan_help,
    EN("Looks through the runs folder for training runs whose config.json names "
       "this partition file, newest per part."),
    JA("学習出力フォルダから、config.json がこの分割ファイルを指している学習を探"
       "し、パートごとに最新のものを使います。"),
    ZH_HANS("在训练输出文件夹中查找 config.json 指向此分区文件的训练，每个分区取最新的。"),
    ZH_HANT("在訓練輸出資料夾中尋找 config.json 指向此分區檔案的訓練，每個分區取最新的。"),
    KO("학습 출력 폴더에서 config.json이 이 분할 파일을 가리키는 학습을 찾아 파트마다 "
       "가장 새 것을 씁니다."),
    DE("Durchsucht den Ordner der Läufe nach Trainingsläufen, deren config.json "
       "diese Partitionsdatei nennt, je Teil den neuesten."),
    FR("Parcourt le dossier des entraînements à la recherche de ceux dont le "
       "config.json nomme ce fichier de partition, le plus récent par partie."),
    ES("Recorre la carpeta de ejecuciones buscando las que nombran este archivo de "
       "partición en su config.json, la más reciente por parte."),
    PT("Percorre a pasta dos treinos à procura dos que nomeiam este ficheiro de "
       "partição no seu config.json, o mais recente por parte."),
    IT("Scorre la cartella delle esecuzioni cercando quelle il cui config.json "
       "nomina questo file di partizione, la più recente per parte."),
    NL("Doorzoekt de map met runs naar trainingsruns waarvan de config.json dit "
       "partitiebestand noemt, de nieuwste per deel."),
    RU("Просматривает папку запусков в поиске тех, чей config.json называет этот "
       "файл разбиения, по новейшему на часть."),
    TR("Çalıştırma klasöründe config.json'u bu bölümleme dosyasını gösteren "
       "eğitimleri arar, parça başına en yenisini alır."));
SS_MSG(scan_result,
    EN("Parts with a model: {0} of {1}"),
    JA("モデルのあるパート: {1} 中 {0}"),
    ZH_HANS("有模型的分区：{0} / {1}"), ZH_HANT("有模型的分區：{0} / {1}"),
    KO("모델이 있는 파트: {1} 중 {0}"), DE("Teile mit Modell: {0} von {1}"),
    FR("Parties avec un modèle : {0} sur {1}"), ES("Partes con modelo: {0} de {1}"),
    PT("Partes com modelo: {0} de {1}"), IT("Parti con un modello: {0} di {1}"),
    NL("Delen met een model: {0} van {1}"), RU("Частей с моделью: {0} из {1}"),
    TR("Modeli olan parça: {1} içinde {0}"));
SS_MSG(run_missing,
    EN("(none)"), JA("（なし）"), ZH_HANS("（无）"), ZH_HANT("（無）"), KO("(없음)"),
    DE("(keins)"), FR("(aucun)"), ES("(ninguno)"), PT("(nenhum)"), IT("(nessuno)"),
    NL("(geen)"), RU("(нет)"), TR("(yok)"));
SS_MSG(btn_merge,
    EN("Merge models"), JA("モデルを結合"), ZH_HANS("合并模型"), ZH_HANT("合併模型"),
    KO("모델 병합"), DE("Modelle zusammenführen"), FR("Fusionner les modèles"),
    ES("Fusionar modelos"), PT("Fundir modelos"), IT("Unisci modelli"),
    NL("Modellen samenvoegen"), RU("Объединить модели"), TR("Modelleri birleştir"));
SS_MSG(merge_done,
    EN("Merged: {0}   splats {1}"), JA("結合しました: {0}   スプラット {1}"),
    ZH_HANS("已合并：{0}   泼溅 {1}"), ZH_HANT("已合併：{0}   潑濺 {1}"),
    KO("병합했습니다: {0}   스플랫 {1}"), DE("Zusammengeführt: {0}   Splats {1}"),
    FR("Fusionné : {0}   splats {1}"), ES("Fusionado: {0}   splats {1}"),
    PT("Fundido: {0}   splats {1}"), IT("Unito: {0}   splat {1}"),
    NL("Samengevoegd: {0}   splats {1}"), RU("Объединено: {0}   сплатов {1}"),
    TR("Birleştirildi: {0}   splat {1}"));
SS_MSG(btn_open_merged,
    EN("Open in viewer"), JA("ビューアで開く"), ZH_HANS("在查看器中打开"),
    ZH_HANT("在檢視器中開啟"), KO("뷰어에서 열기"), DE("Im Betrachter öffnen"),
    FR("Ouvrir dans la visionneuse"), ES("Abrir en el visor"), PT("Abrir no visualizador"),
    IT("Apri nel visualizzatore"), NL("Openen in de viewer"), RU("Открыть в просмотрщике"),
    TR("Görüntüleyicide aç"));
SS_MSG(stage_merge,
    EN("Merge"), JA("結合"), ZH_HANS("合并"), ZH_HANT("合併"), KO("병합"),
    DE("Zusammenführen"), FR("Fusionner"), ES("Fusionar"), PT("Fundir"), IT("Unisci"),
    NL("Samenvoegen"), RU("Объединить"), TR("Birleştir"));
SS_MSG(plan_merge,
    EN("{0}. Merge the trained parts of {1}"),
    JA("{0}. {1} の学習済みパートを結合する"),
    ZH_HANS("{0}. 合并 {1} 已训练的各分区"),
    ZH_HANT("{0}. 合併 {1} 已訓練的各分區"),
    KO("{0}. {1}의 학습된 파트를 병합"),
    DE("{0}. Die trainierten Teile von {1} zusammenführen"),
    FR("{0}. Fusionner les parties entraînées de {1}"),
    ES("{0}. Fusionar las partes entrenadas de {1}"),
    PT("{0}. Fundir as partes treinadas de {1}"),
    IT("{0}. Unire le parti addestrate di {1}"),
    NL("{0}. De getrainde delen van {1} samenvoegen"),
    RU("{0}. Объединить обученные части {1}"),
    TR("{0}. {1} için eğitilmiş parçaları birleştir"));
SS_MSG(batch_log_merge,
    EN("[{0}] Merging the parts of {1}"),
    JA("[{0}] {1} のパートを結合しています"),
    ZH_HANS("[{0}] 正在合并 {1} 的各分区"),
    ZH_HANT("[{0}] 正在合併 {1} 的各分區"),
    KO("[{0}] {1}의 파트를 병합하는 중"),
    DE("[{0}] Die Teile von {1} werden zusammengeführt"),
    FR("[{0}] Fusion des parties de {1}"),
    ES("[{0}] Fusionando las partes de {1}"),
    PT("[{0}] A fundir as partes de {1}"),
    IT("[{0}] Unione delle parti di {1}"),
    NL("[{0}] De delen van {1} worden samengevoegd"),
    RU("[{0}] Объединение частей {1}"),
    TR("[{0}] {1} parçaları birleştiriliyor"));
SS_MSG(merge_row_help,
    EN("Runs after the rows above it: every part of this partition that has a "
       "trained model in the runs folder is joined into one file written there."),
    JA("上の行のあとに実行されます。この分割のうち、出力フォルダに学習済みモデル"
       "があるパートをすべて結合し、そこにひとつのファイルとして書き出します。"),
    ZH_HANS("在上方各行之后运行：把此分区中所有在输出文件夹里有训练模型的分区合并成"
            "一个文件并写在那里。"),
    ZH_HANT("在上方各列之後執行：把此分區中所有在輸出資料夾裡有訓練模型的分區合併成"
            "一個檔案並寫在那裡。"),
    KO("위 행들 다음에 실행됩니다. 이 분할에서 출력 폴더에 학습된 모델이 있는 모든 "
       "파트를 하나의 파일로 합쳐 그곳에 씁니다."),
    DE("Läuft nach den Zeilen darüber: Jeder Teil dieser Partition, der im "
       "Ordner der Läufe ein trainiertes Modell hat, wird zu einer Datei dort "
       "zusammengeführt."),
    FR("S'exécute après les lignes du dessus : chaque partie de cette partition "
       "ayant un modèle entraîné dans le dossier des entraînements est réunie en "
       "un fichier écrit là."),
    ES("Se ejecuta tras las filas de arriba: cada parte de esta partición con un "
       "modelo entrenado en la carpeta de ejecuciones se une en un archivo "
       "escrito allí."),
    PT("Corre depois das linhas acima: cada parte desta partição com um modelo "
       "treinado na pasta dos treinos é reunida num ficheiro escrito lá."),
    IT("Viene eseguito dopo le righe sopra: ogni parte di questa partizione con "
       "un modello addestrato nella cartella delle esecuzioni è riunita in un "
       "file scritto lì."),
    NL("Draait na de rijen erboven: elk deel van deze partitie met een getraind "
       "model in de map met runs wordt tot één bestand daar samengevoegd."),
    RU("Выполняется после строк выше: каждая часть этого разбиения, у которой в "
       "папке запусков есть обученная модель, объединяется в один файл там же."),
    TR("Üstteki satırlardan sonra çalışır: bu bölümlemenin çalıştırma "
       "klasöründe eğitilmiş modeli olan her parçası orada tek bir dosyada "
       "birleştirilir."));
SS_MSG(btn_open_batch,
    EN("Open Batch Processing"), JA("バッチ処理を開く"), ZH_HANS("打开批处理"),
    ZH_HANT("開啟批次處理"), KO("배치 처리 열기"), DE("Stapelverarbeitung öffnen"),
    FR("Ouvrir le traitement par lot"), ES("Abrir procesamiento por lotes"),
    PT("Abrir processamento em lote"), IT("Apri elaborazione in lotto"),
    NL("Batchverwerking openen"), RU("Открыть пакетную обработку"),
    TR("Toplu işlemeyi aç"));
SS_MSG(pq_title,
    EN("Queue the parts"), JA("パートをキューに入れる"), ZH_HANS("将分区排入队列"),
    ZH_HANT("將分區排入佇列"), KO("파트를 큐에 넣기"), DE("Teile einreihen"),
    FR("Mettre les parties en file"), ES("Poner las partes en cola"),
    PT("Pôr as partes em fila"), IT("Metti le parti in coda"),
    NL("Delen in de wachtrij zetten"), RU("Поставить части в очередь"),
    TR("Parçaları sıraya al"));
SS_MSG(pq_runs,
    EN("Training runs to queue: {0}, one per part of {1}"),
    JA("キューに入れる学習: {0} 件（{1} のパートごとに 1 件）"),
    ZH_HANS("将排入的训练：{0} 次，{1} 的每个分区一次"),
    ZH_HANT("將排入的訓練：{0} 次，{1} 的每個分區一次"),
    KO("큐에 넣을 학습: {0}개, {1}의 파트마다 하나"),
    DE("Einzureihende Trainingsläufe: {0}, einer je Teil von {1}"),
    FR("Entraînements à mettre en file : {0}, un par partie de {1}"),
    ES("Ejecuciones a poner en cola: {0}, una por parte de {1}"),
    PT("Treinos a pôr em fila: {0}, um por parte de {1}"),
    IT("Esecuzioni da mettere in coda: {0}, una per parte di {1}"),
    NL("Trainingsruns in de wachtrij: {0}, één per deel van {1}"),
    RU("Запусков обучения в очередь: {0}, по одному на часть {1}"),
    TR("Sıraya alınacak eğitim: {0}, {1} parçası başına bir"));
SS_MSG(pq_merge,
    EN("Merge the parts once all have trained"),
    JA("すべて学習し終えたらパートを結合する"),
    ZH_HANS("全部训练完成后合并各分区"), ZH_HANT("全部訓練完成後合併各分區"),
    KO("모두 학습되면 파트를 병합"), DE("Die Teile zusammenführen, sobald alle trainiert sind"),
    FR("Fusionner les parties une fois toutes entraînées"),
    ES("Fusionar las partes cuando todas hayan entrenado"),
    PT("Fundir as partes quando todas tiverem treinado"),
    IT("Unire le parti quando tutte sono addestrate"),
    NL("De delen samenvoegen zodra alle getraind zijn"),
    RU("Объединить части, когда все обучены"),
    TR("Hepsi eğitilince parçaları birleştir"));
SS_MSG(pq_merge_help,
    EN("Adds a last row to the list that joins the parts' models into one file "
       "in the runs folder, written beside the dataset."),
    JA("一覧の最後に、各パートのモデルをひとつのファイルに結合してデータセットの隣"
       "の出力フォルダに書き出す行を追加します。"),
    ZH_HANS("在列表末尾加一行，把各分区的模型合并成一个文件，写到数据集旁的输出文件夹。"),
    ZH_HANT("在列表末尾加一列，把各分區的模型合併成一個檔案，寫到資料集旁的輸出資料夾。"),
    KO("목록 끝에 각 파트의 모델을 하나의 파일로 합쳐 데이터셋 옆 출력 폴더에 쓰는 "
       "행을 추가합니다."),
    DE("Hängt eine letzte Zeile an, die die Modelle der Teile zu einer Datei im "
       "Ordner der Läufe neben dem Datensatz zusammenführt."),
    FR("Ajoute une dernière ligne qui réunit les modèles des parties en un fichier "
       "dans le dossier des entraînements, à côté du jeu de données."),
    ES("Añade una última fila que une los modelos de las partes en un archivo en "
       "la carpeta de ejecuciones, junto al conjunto de datos."),
    PT("Acrescenta uma última linha que reúne os modelos das partes num ficheiro "
       "na pasta dos treinos, ao lado do conjunto de dados."),
    IT("Aggiunge un'ultima riga che riunisce i modelli delle parti in un file "
       "nella cartella delle esecuzioni, accanto al set di dati."),
    NL("Voegt een laatste rij toe die de modellen van de delen tot één bestand in "
       "de map met runs naast de dataset samenvoegt."),
    RU("Добавляет последнюю строку, которая объединяет модели частей в один файл "
       "в папке запусков рядом с набором данных."),
    TR("Listeye, parçaların modellerini veri kümesinin yanındaki çalıştırma "
       "klasöründe tek dosyada birleştiren son bir satır ekler."));
SS_MSG(pq_queue,
    EN("Queue"), JA("キューに入れる"), ZH_HANS("排入队列"), ZH_HANT("排入佇列"),
    KO("큐에 넣기"), DE("Einreihen"), FR("Mettre en file"), ES("Poner en cola"),
    PT("Pôr em fila"), IT("Metti in coda"), NL("In de wachtrij"), RU("В очередь"),
    TR("Sıraya al"));
SS_MSG(pq_pending,
    EN("Rows on the batch list that have not finished: {0}. Clear them before "
       "queuing the parts?"),
    JA("バッチ一覧にまだ終わっていない行があります: {0}。パートを入れる前にそれら"
       "を消しますか？"),
    ZH_HANS("批处理列表中还有 {0} 行尚未完成。排入分区之前先清除它们吗？"),
    ZH_HANT("批次處理列表中還有 {0} 列尚未完成。排入分區之前先清除它們嗎？"),
    KO("배치 목록에 아직 끝나지 않은 행이 {0}개 있습니다. 파트를 넣기 전에 지울까요?"),
    DE("Zeilen auf der Stapelliste, die nicht fertig sind: {0}. Vor dem Einreihen "
       "der Teile entfernen?"),
    FR("Lignes de la liste non terminées : {0}. Les retirer avant de mettre les "
       "parties en file ?"),
    ES("Filas de la lista que no han terminado: {0}. ¿Eliminarlas antes de poner "
       "las partes en cola?"),
    PT("Linhas da lista que não terminaram: {0}. Removê-las antes de pôr as "
       "partes em fila?"),
    IT("Righe della lista non terminate: {0}. Rimuoverle prima di mettere le "
       "parti in coda?"),
    NL("Rijen op de batchlijst die niet klaar zijn: {0}. Verwijderen voordat de "
       "delen in de wachtrij gaan?"),
    RU("Незавершённых строк в пакетном списке: {0}. Убрать их перед постановкой "
       "частей в очередь?"),
    TR("Toplu listede bitmemiş satır: {0}. Parçaları sıraya almadan önce "
       "temizlensin mi?"));
SS_MSG(pq_clear,
    EN("Clear them, then queue"), JA("消してから入れる"), ZH_HANS("清除后再排入"),
    ZH_HANT("清除後再排入"), KO("지우고 넣기"), DE("Entfernen, dann einreihen"),
    FR("Les retirer, puis mettre en file"), ES("Eliminarlas y poner en cola"),
    PT("Removê-las e pôr em fila"), IT("Rimuoverle, poi in coda"),
    NL("Verwijderen, dan in de wachtrij"), RU("Убрать и поставить"),
    TR("Temizle, sonra sıraya al"));
SS_MSG(pq_keep,
    EN("Keep them, queue after"), JA("残したまま後ろに入れる"), ZH_HANS("保留并排在其后"),
    ZH_HANT("保留並排在其後"), KO("남겨 두고 뒤에 넣기"), DE("Behalten, danach einreihen"),
    FR("Les garder, mettre en file après"), ES("Conservarlas y poner detrás"),
    PT("Mantê-las e pôr a seguir"), IT("Tenerle, in coda dopo"),
    NL("Bewaren, erachter in de wachtrij"), RU("Оставить, поставить после"),
    TR("Bırak, arkasına sıraya al"));

SS_MSG(legend,
    EN("Frusta take their part's colour: solid for a part's own cameras, faint "
       "for the ring it borrows. Points take their owner's colour."),
    JA("フラスタムは所属パートの色で表示します。パート自身のカメラは実線、借りる"
       "リングは薄く。点は所有パートの色です。"),
    ZH_HANS("视锥按所属分区着色：自身相机为实线，借用的外环为淡色。点按归属分区着色。"),
    ZH_HANT("視錐按所屬分區著色：自身相機為實線，借用的外環為淡色。點按歸屬分區著色。"),
    KO("절두체는 파트 색을 띱니다. 파트 자신의 카메라는 진하게, 빌린 고리는 흐리게. "
       "점은 소유 파트 색입니다."),
    DE("Frusta tragen die Farbe ihres Teils: kräftig für eigene Kameras, blass für "
       "den geliehenen Ring. Punkte tragen die Farbe ihres Besitzers."),
    FR("Les frustums prennent la couleur de leur partie : plein pour ses propres "
       "caméras, pâle pour l'anneau emprunté. Les points prennent la couleur de "
       "leur partie."),
    ES("Los frustos toman el color de su parte: sólido para sus propias cámaras, "
       "tenue para el anillo prestado. Los puntos toman el color de su parte."),
    PT("Os frustos tomam a cor da sua parte: cheio para as próprias câmaras, "
       "esbatido para o anel emprestado. Os pontos tomam a cor da sua parte."),
    IT("I frustum prendono il colore della loro parte: pieno per le proprie "
       "fotocamere, tenue per l'anello preso in prestito. I punti prendono il "
       "colore della loro parte."),
    NL("Frusta krijgen de kleur van hun deel: vol voor eigen camera's, vaag voor "
       "de geleende ring. Punten krijgen de kleur van hun eigenaar."),
    RU("Пирамиды видимости окрашены по части: ярко -- свои камеры, бледно -- "
       "заимствованное кольцо. Точки окрашены по владельцу."),
    TR("Görüş piramitleri parçasının rengini alır: kendi kameraları dolu, ödünç "
       "halka soluk. Noktalar sahibinin rengini alır."));

}  // namespace partition
}  // namespace msg
}  // namespace i18n
}  // namespace spirula

#include "i18n/EndCatalog.h"
