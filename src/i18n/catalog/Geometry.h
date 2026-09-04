#pragma once

// What `spirula geometry` says: its --help and every line a run prints.
//
// Flag names, the words they accept (`auto`, `png`, `mm`) and the model ids
// are identifiers and stay as they are in every language -- they are what the
// reader types. What is translated is the sentence beside each one.

#include "i18n/BeginCatalog.h"

namespace spirula {
namespace i18n {
namespace msg {
namespace geometry {

SS_MSG(tagline,
    EN("Depth and surface normals for a dataset, from a single image each"),
    JA("各画像 1 枚から、データセットの深度と面法線を推定します"),
    ZH_HANS("从每张单幅图像估计数据集的深度与表面法线"),
    ZH_HANT("從每張單幅影像估計資料集的深度與表面法線"),
    KO("이미지 한 장씩으로 데이터셋의 깊이와 표면 법선을 추정합니다"),
    DE("Tiefe und Oberflächennormalen für einen Datensatz, je aus einem Bild"),
    FR("Profondeur et normales de surface d'un jeu de données, image par image"),
    ES("Profundidad y normales de superficie de un conjunto, imagen a imagen"),
    PT("Profundidade e normais de superfície de um conjunto, imagem a imagem"),
    IT("Profondità e normali di superficie di un insieme, immagine per immagine"),
    NL("Diepte en oppervlaktenormalen voor een dataset, per losse afbeelding"),
    RU("Глубина и нормали поверхности для набора, по одному кадру за раз"),
    TR("Bir veri kümesi için derinlik ve yüzey normalleri, tek tek görüntülerden"));

SS_MSG(usage_target,
    EN("The dataset folder: a transforms.json, a COLMAP sparse/ model or a "
       "Metashape export. Normals are written to normals/ and depth to "
       "depths/, which both dataset readers find by name -- nothing rewrites "
       "your transforms.json."),
    JA("データセットのフォルダです。transforms.json、COLMAP の sparse/ モデル、"
       "または Metashape の書き出しを読みます。法線は normals/ に、深度は "
       "depths/ に書き出します。どちらも名前で見つかるので、transforms.json は"
       "書き換えません。"),
    ZH_HANS("数据集文件夹：transforms.json、COLMAP 的 sparse/ 模型或 Metashape "
            "导出。法线写入 normals/，深度写入 depths/，两个读取器都按名字查找，"
            "所以不会改写你的 transforms.json。"),
    ZH_HANT("資料集資料夾：transforms.json、COLMAP 的 sparse/ 模型或 Metashape "
            "匯出。法線寫入 normals/，深度寫入 depths/，兩個讀取器都按名字尋找，"
            "所以不會改寫你的 transforms.json。"),
    KO("데이터셋 폴더입니다. transforms.json, COLMAP sparse/ 모델, Metashape "
       "내보내기를 읽습니다. 법선은 normals/, 깊이는 depths/ 에 씁니다. 두 "
       "판독기 모두 이름으로 찾으므로 transforms.json 은 고치지 않습니다."),
    DE("Der Datensatzordner: eine transforms.json, ein COLMAP-sparse/-Modell "
       "oder ein Metashape-Export. Normalen landen in normals/, Tiefe in "
       "depths/; beide Leser finden sie am Namen, Ihre transforms.json bleibt "
       "unangetastet."),
    FR("Le dossier du jeu de données : un transforms.json, un modèle COLMAP "
       "sparse/ ou un export Metashape. Les normales vont dans normals/ et la "
       "profondeur dans depths/, que les deux lecteurs trouvent par leur nom : "
       "votre transforms.json n'est pas réécrit."),
    ES("La carpeta del conjunto: un transforms.json, un modelo COLMAP sparse/ "
       "o una exportación de Metashape. Las normales van a normals/ y la "
       "profundidad a depths/, que ambos lectores encuentran por su nombre: tu "
       "transforms.json no se reescribe."),
    PT("A pasta do conjunto: um transforms.json, um modelo COLMAP sparse/ ou "
       "uma exportação do Metashape. As normais vão para normals/ e a "
       "profundidade para depths/, que ambos os leitores encontram pelo nome: "
       "o seu transforms.json não é reescrito."),
    IT("La cartella dell'insieme: un transforms.json, un modello COLMAP "
       "sparse/ o un'esportazione Metashape. Le normali vanno in normals/ e la "
       "profondità in depths/, che entrambi i lettori trovano per nome: il tuo "
       "transforms.json non viene riscritto."),
    NL("De datasetmap: een transforms.json, een COLMAP-sparse/-model of een "
       "Metashape-export. Normalen gaan naar normals/ en diepte naar depths/, "
       "die beide lezers op naam vinden -- je transforms.json blijft ongemoeid."),
    RU("Папка набора: transforms.json, модель COLMAP sparse/ или выгрузка "
       "Metashape. Нормали пишутся в normals/, глубина в depths/ — оба "
       "считывателя находят их по имени, и ваш transforms.json не переписывается."),
    TR("Veri kümesi klasörü: bir transforms.json, bir COLMAP sparse/ modeli ya "
       "da bir Metashape dışa aktarımı. Normaller normals/, derinlik depths/ "
       "klasörüne yazılır; iki okuyucu da bunları adıyla bulur, "
       "transforms.json dosyanız yeniden yazılmaz."));

SS_MSG(head_options,
    EN("Options"), JA("オプション"), ZH_HANS("选项"), ZH_HANT("選項"),
    KO("옵션"), DE("Optionen"), FR("Options"), ES("Opciones"), PT("Opções"),
    IT("Opzioni"), NL("Opties"), RU("Параметры"), TR("Seçenekler"));

SS_MSG(opt_image_dir,
    EN("The folder holding the images, relative to the dataset or absolute. "
       "Default: images"),
    JA("画像が置かれているフォルダ。データセットからの相対パスまたは絶対パス。"
       "既定: images"),
    ZH_HANS("存放图像的文件夹，相对于数据集或绝对路径。默认：images"),
    ZH_HANT("存放影像的資料夾，相對於資料集或絕對路徑。預設：images"),
    KO("이미지가 있는 폴더. 데이터셋 기준 상대 경로 또는 절대 경로. "
       "기본값: images"),
    DE("Der Ordner mit den Bildern, relativ zum Datensatz oder absolut. "
       "Standard: images"),
    FR("Le dossier contenant les images, relatif au jeu de données ou absolu. "
       "Par défaut : images"),
    ES("La carpeta con las imágenes, relativa al conjunto o absoluta. "
       "Por defecto: images"),
    PT("A pasta com as imagens, relativa ao conjunto ou absoluta. "
       "Predefinição: images"),
    IT("La cartella con le immagini, relativa all'insieme o assoluta. "
       "Predefinito: images"),
    NL("De map met de beelden, relatief aan de dataset of absoluut. "
       "Standaard: images"),
    RU("Папка с изображениями, относительно набора или абсолютная. "
       "По умолчанию: images"),
    TR("Görüntülerin bulunduğu klasör; veri kümesine göre göreli ya da mutlak. "
       "Varsayılan: images"));

SS_MSG(opt_model,
    EN("Which checkpoint to run. A known id is fetched and cached; a path to "
       "an .onnx file is used as it is. Larger is slower and only somewhat "
       "better."),
    JA("使うチェックポイントです。既知の ID は取得してキャッシュし、.onnx への"
       "パスはそのまま使います。大きいほど遅く、精度の伸びはわずかです。"),
    ZH_HANS("要运行的权重。已知 id 会自动下载并缓存；给出 .onnx 路径则直接使用。"
            "越大越慢，精度提升有限。"),
    ZH_HANT("要執行的權重。已知 id 會自動下載並快取；給出 .onnx 路徑則直接使用。"
            "越大越慢，精度提升有限。"),
    KO("실행할 체크포인트입니다. 알려진 id 는 받아서 캐시하고, .onnx 경로는 그대로 "
       "씁니다. 클수록 느리고 정확도 향상은 크지 않습니다."),
    DE("Welche Gewichte laufen sollen. Eine bekannte Kennung wird geholt und "
       "zwischengespeichert, ein Pfad auf eine .onnx unverändert benutzt. "
       "Größer heißt langsamer und nur etwas besser."),
    FR("Quel jeu de poids exécuter. Un identifiant connu est téléchargé puis "
       "mis en cache ; un chemin vers un .onnx est utilisé tel quel. Plus gros "
       "est plus lent et à peine meilleur."),
    ES("Qué pesos ejecutar. Un identificador conocido se descarga y se guarda "
       "en caché; una ruta a un .onnx se usa tal cual. Más grande es más lento "
       "y solo algo mejor."),
    PT("Que pesos executar. Um identificador conhecido é obtido e guardado em "
       "cache; um caminho para um .onnx é usado tal como está. Maior é mais "
       "lento e só um pouco melhor."),
    IT("Quali pesi eseguire. Un identificatore noto viene scaricato e messo in "
       "cache; un percorso a un .onnx è usato così com'è. Più grande è più "
       "lento e solo un po' migliore."),
    NL("Welke gewichten draaien. Een bekende id wordt opgehaald en bewaard; "
       "een pad naar een .onnx wordt gebruikt zoals het is. Groter is trager "
       "en maar iets beter."),
    RU("Какие веса запускать. Известный идентификатор скачивается и "
       "кэшируется, путь к .onnx берётся как есть. Крупнее — медленнее и лишь "
       "немного точнее."),
    TR("Hangi ağırlıkların çalışacağı. Bilinen bir kimlik indirilip "
       "önbelleğe alınır; bir .onnx yolu olduğu gibi kullanılır. Büyüğü daha "
       "yavaştır ve yalnızca biraz daha iyidir."));

SS_MSG(opt_max_size,
    EN("Longest side of one image the network runs, and of the maps written. "
       "A split sizes its faces off the full frame and meets this only as a "
       "ceiling, so a wide capture still runs most of the detail it was shot "
       "with. 1064 is what the model's own pipeline uses."),
    JA("ネットワークが 1 枚で処理する画像と、書き出すマップの長辺で"
       "す。分割時の面は元の解像度から大きさを決め、これは上限としてだけ効"
       "きます。1064 はモデル本来のパイプラインが使う大きさです。"),
    ZH_HANS("网络单张处理的图像、以及写出贴图的长边。拆分时各面的大小由原"
            "分辨率决定，这里只作为上限，因此超广拍摄仍能保留大部分细节。"
            "1064 是该模型自身流程使用的大小。"),
    ZH_HANT("網路單張處理的影像、以及寫出貼圖的長邊。拆分時各面的大小由原"
            "解析度決定，這裡只作為上限，因此超廣拍攝仍能保留大部分細節。"
            "1064 是該模型自身流程使用的大小。"),
    KO("신경망이 한 번에 처리하는 이미지와 기록하는 맵의 긴 변입니"
       "다. 분할한 면의 크기는 원본 해상도에서 정해지고 이 값은 상한으로만 "
       "쓰입니다. 1064 는 모델 자체 파이프라인이 쓰는 크기입니다."),
    DE("Längste Seite eines Bildes, das das Netz verarbeitet, und der "
       "geschriebenen Karten. Eine Zerlegung bemisst ihre Flächen am vollen "
       "Bild und trifft dies nur als Obergrenze, eine weite Aufnahme läuft "
       "also mit fast allen Details. 1064 nutzt die Pipeline des Modells."),
    FR("Plus grand côté d'une image traitée par le réseau, et des cartes "
       "écrites. Un découpage dimensionne ses faces sur l'image pleine et ne "
       "rencontre ceci que comme plafond : une prise très ouverte garde donc "
       "presque tout son détail. 1064 est ce qu'utilise le pipeline du modèle."),
    ES("Lado mayor de una imagen que procesa la red, y de los mapas escritos. "
       "Una división dimensiona sus caras sobre el cuadro completo y solo "
       "encuentra esto como techo, así que una toma muy abierta conserva casi "
       "todo su detalle. 1064 es lo que usa la tubería del modelo."),
    PT("Maior lado de uma imagem que a rede processa, e dos mapas escritos. "
       "Uma divisão dimensiona as suas faces pelo quadro completo e encontra "
       "isto apenas como tecto, pelo que uma captura muito aberta mantém quase "
       "todo o detalhe. 1064 é o que o pipeline do modelo usa."),
    IT("Lato più lungo di un'immagine che la rete elabora, e delle mappe "
       "scritte. Una divisione dimensiona le sue facce sul fotogramma intero e "
       "incontra questo solo come tetto, quindi una ripresa molto ampia "
       "conserva quasi tutto il dettaglio. 1064 è ciò che usa la pipeline del "
       "modello."),
    NL("Langste zijde van één beeld dat het netwerk verwerkt, en van de "
       "geschreven kaarten. Een splitsing bemeet haar vlakken op het volledige "
       "beeld en komt dit alleen als plafond tegen, dus een zeer wijde opname "
       "houdt bijna al haar detail. 1064 gebruikt de pijplijn van het model."),
    RU("Наибольшая сторона одного изображения для сети и "
       "записываемых карт. При разбиении грани берут размер от "
       "полного кадра, а это лишь потолок, так что широкая съёмка "
       "сохраняет почти весь детализм. 1064 — размер конвейера модели."),
    TR("Ağın işlediği bir görüntünün ve yazılan haritaların en uzun "
       "kenarı. Bölme, yüzlerini tam kareye göre boyutlandırır ve bunu yalnızca "
       "tavan olarak görür; çok geniş bir çekim ayrıntısının çoğunu korur. "
       "1064 modelin kendi işlem hattının kullandığı boyuttur."));

SS_MSG(opt_num_tokens,
    EN("How many patches MoGe's transformer runs, which is what sets its cost "
       "rather than the image size. Its own range is 1200 to 3600, and it is "
       "capped at what the image holds. Metric3D ignores this."),
    JA("MoGe のトランスフォーマーが処理するパッチ数です。画像の大きさではなく"
       "これが処理量を決めます。本来の範囲は 1200 から 3600 で、画像が持つ数が"
       "上限になります。Metric3D では無視されます。"),
    ZH_HANS("MoGe 的 Transformer 处理多少个图块，决定其开销的是这个而不是图像"
            "大小。其自身范围是 1200 到 3600，并以图像所含的数量为上限。"
            "Metric3D 会忽略此项。"),
    ZH_HANT("MoGe 的 Transformer 處理多少個圖塊，決定其開銷的是這個而不是影像"
            "大小。其自身範圍是 1200 到 3600，並以影像所含的數量為上限。"
            "Metric3D 會忽略此項。"),
    KO("MoGe 의 트랜스포머가 처리하는 패치 수입니다. 이미지 크기가 아니라 이 "
       "값이 비용을 정합니다. 자체 범위는 1200 에서 3600 이고 이미지가 가진 "
       "수가 상한입니다. Metric3D 는 무시합니다."),
    DE("Wie viele Kacheln MoGes Transformer verarbeitet; das bestimmt die "
       "Kosten, nicht die Bildgröße. Sein eigener Bereich ist 1200 bis 3600, "
       "begrenzt durch das, was das Bild hergibt. Metric3D ignoriert dies."),
    FR("Combien de tuiles le transformeur de MoGe traite : c'est cela qui fixe "
       "son coût, pas la taille de l'image. Sa plage propre va de 1200 à 3600, "
       "plafonnée par ce que contient l'image. Metric3D l'ignore."),
    ES("Cuántos parches procesa el transformador de MoGe, que es lo que fija su "
       "coste y no el tamaño de la imagen. Su rango propio va de 1200 a 3600, "
       "limitado por lo que la imagen contiene. Metric3D lo ignora."),
    PT("Quantos blocos o transformador do MoGe processa, que é o que define o "
       "seu custo e não o tamanho da imagem. O seu intervalo é de 1200 a 3600, "
       "limitado pelo que a imagem contém. O Metric3D ignora isto."),
    IT("Quante patch elabora il transformer di MoGe: è questo a fissarne il "
       "costo, non la dimensione dell'immagine. Il suo intervallo va da 1200 a "
       "3600, limitato da quanto contiene l'immagine. Metric3D lo ignora."),
    NL("Hoeveel vlakjes MoGe's transformer verwerkt; dat bepaalt de kosten, "
       "niet de beeldgrootte. Zijn eigen bereik is 1200 tot 3600, begrensd "
       "door wat het beeld bevat. Metric3D negeert dit."),
    RU("Сколько фрагментов обрабатывает трансформер MoGe — именно это задаёт "
       "стоимость, а не размер изображения. Собственный диапазон от 1200 до "
       "3600, с потолком по числу в изображении. Metric3D это игнорирует."),
    TR("MoGe'nin dönüştürücüsünün işlediği yama sayısı; maliyeti görüntü "
       "boyutu değil bu belirler. Kendi aralığı 1200 ile 3600 arasıdır ve "
       "görüntünün içerdiğiyle sınırlanır. Metric3D bunu yok sayar."));

SS_MSG(opt_depth,
    EN("Also write depth maps. Off by default: the normals are what a "
       "reconstruction usually wants, and depth doubles both the time on disk "
       "and the reading a training run does."),
    JA("深度マップも書き出します。既定は無効です。再構成でふつう効くのは法線で、"
       "深度はディスク上の容量も学習時の読み込みも倍にします。"),
    ZH_HANS("同时写出深度图。默认关闭：重建通常真正用得上的是法线，而深度会让"
            "磁盘占用和训练时的读取都翻倍。"),
    ZH_HANT("同時寫出深度圖。預設關閉：重建通常真正用得上的是法線，而深度會讓"
            "磁碟佔用和訓練時的讀取都翻倍。"),
    KO("깊이 맵도 함께 씁니다. 기본은 꺼짐입니다. 재구성에서 주로 쓰이는 것은 "
       "법선이고, 깊이는 디스크 용량과 학습 때의 읽기를 모두 두 배로 만듭니다."),
    DE("Auch Tiefenkarten schreiben. Standardmäßig aus: für eine "
       "Rekonstruktion zählen meist die Normalen, und Tiefe verdoppelt sowohl "
       "den Platz auf der Platte als auch das Lesen im Training."),
    FR("Écrire aussi les cartes de profondeur. Désactivé par défaut : une "
       "reconstruction se sert surtout des normales, et la profondeur double "
       "à la fois la place sur le disque et la lecture d'un entraînement."),
    ES("Escribir también mapas de profundidad. Desactivado por defecto: una "
       "reconstrucción suele usar las normales, y la profundidad duplica tanto "
       "el espacio en disco como la lectura de un entrenamiento."),
    PT("Escrever também mapas de profundidade. Desligado por omissão: uma "
       "reconstrução usa sobretudo as normais, e a profundidade duplica tanto "
       "o espaço em disco como a leitura de um treino."),
    IT("Scrivere anche le mappe di profondità. Spento per impostazione "
       "predefinita: una ricostruzione usa soprattutto le normali, e la "
       "profondità raddoppia sia lo spazio su disco sia la lettura di un "
       "addestramento."),
    NL("Ook diepteafbeeldingen schrijven. Standaard uit: een reconstructie "
       "gebruikt meestal de normalen, en diepte verdubbelt zowel de "
       "schijfruimte als het lezen tijdens een training."),
    RU("Также записывать карты глубины. По умолчанию выключено: реконструкции "
       "обычно нужны нормали, а глубина удваивает и место на диске, и чтение "
       "во время обучения."),
    TR("Derinlik haritalarını da yaz. Öntanımlı kapalı: bir yeniden "
       "oluşturmada işe yarayan genelde normallerdir ve derinlik hem diskteki "
       "yeri hem de eğitimdeki okumayı ikiye katlar."));

SS_MSG(opt_normal_format,
    EN("File type for the normal maps. `jpg` is about a fifth the size; the "
       "loss lands on the normal's direction, which the geometry term reads "
       "directly."),
    JA("法線マップの形式です。`jpg` は容量が約 5 分の 1 になりますが、失われる"
       "のは法線の向きそのもので、ジオメトリ項はそれを直接読みます。"),
    ZH_HANS("法线图的文件格式。`jpg` 体积约为五分之一，但损失落在法线方向上，"
            "而几何项正是直接读取它。"),
    ZH_HANT("法線圖的檔案格式。`jpg` 體積約為五分之一，但損失落在法線方向上，"
            "而幾何項正是直接讀取它。"),
    KO("법선 맵의 파일 형식입니다. `jpg` 는 크기가 약 5 분의 1 이지만, 손실이 "
       "법선의 방향에 그대로 실리고 기하 항은 그것을 직접 읽습니다."),
    DE("Dateityp der Normalenkarten. `jpg` ist etwa ein Fünftel so groß; der "
       "Verlust trifft die Richtung der Normale, die der Geometrieterm direkt "
       "liest."),
    FR("Type de fichier des cartes de normales. `jpg` pèse environ un "
       "cinquième ; la perte porte sur la direction de la normale, que le "
       "terme géométrique lit directement."),
    ES("Tipo de archivo de los mapas de normales. `jpg` ocupa una quinta parte; "
       "la pérdida cae sobre la dirección de la normal, que el término "
       "geométrico lee directamente."),
    PT("Tipo de ficheiro dos mapas de normais. `jpg` ocupa cerca de um quinto; "
       "a perda recai sobre a direção da normal, que o termo geométrico lê "
       "diretamente."),
    IT("Tipo di file delle mappe di normali. `jpg` occupa circa un quinto; la "
       "perdita cade sulla direzione della normale, che il termine geometrico "
       "legge direttamente."),
    NL("Bestandstype van de normaalafbeeldingen. `jpg` is ongeveer een vijfde "
       "zo groot; het verlies valt op de richting van de normaal, die de "
       "geometrieterm rechtstreeks leest."),
    RU("Формат файлов карт нормалей. `jpg` примерно впятеро меньше, но потери "
       "приходятся на само направление нормали, которое геометрический член "
       "читает напрямую."),
    TR("Normal haritalarının dosya türü. `jpg` yaklaşık beşte bir yer kaplar; "
       "kayıp doğrudan normalin yönüne biner ve geometri terimi onu doğrudan "
       "okur."));

SS_MSG(opt_jpeg_quality,
    EN("JPEG quality for the normal maps, when the format is `jpg`."),
    JA("形式が `jpg` のときの、法線マップの JPEG 品質です。"),
    ZH_HANS("格式为 `jpg` 时，法线图的 JPEG 质量。"),
    ZH_HANT("格式為 `jpg` 時，法線圖的 JPEG 品質。"),
    KO("형식이 `jpg` 일 때 법선 맵의 JPEG 품질입니다."),
    DE("JPEG-Qualität der Normalenkarten, wenn das Format `jpg` ist."),
    FR("Qualité JPEG des cartes de normales, quand le format est `jpg`."),
    ES("Calidad JPEG de los mapas de normales, cuando el formato es `jpg`."),
    PT("Qualidade JPEG dos mapas de normais, quando o formato é `jpg`."),
    IT("Qualità JPEG delle mappe di normali, quando il formato è `jpg`."),
    NL("JPEG-kwaliteit van de normaalafbeeldingen, als het formaat `jpg` is."),
    RU("Качество JPEG для карт нормалей, когда формат — `jpg`."),
    TR("Biçim `jpg` iken normal haritalarının JPEG kalitesi."));

SS_MSG(opt_ray_depth,
    EN("Whether the stored depth is the distance along the ray rather than a z "
       "coordinate. `auto` picks ray depth exactly when the frame was split "
       "into pinhole faces, which is the same call the trainer's "
       "--input-depth-is-ray-depth makes when it is left unset."),
    JA("保存する深度を、z 座標ではなく光線に沿った距離にするかです。`auto` は"
       "ピンホール面に分割したときだけ光線深度を選びます。学習側の "
       "--input-depth-is-ray-depth を未指定にしたときの判断と同じです。"),
    ZH_HANS("保存的深度是沿光线的距离还是 z 坐标。`auto` 仅在画面被拆成针孔面时"
            "选择光线深度，这与训练侧 --input-depth-is-ray-depth 留空时的判断相同。"),
    ZH_HANT("儲存的深度是沿光線的距離還是 z 座標。`auto` 僅在畫面被拆成針孔面時"
            "選擇光線深度，這與訓練側 --input-depth-is-ray-depth 留空時的判斷相同。"),
    KO("저장하는 깊이를 z 좌표 대신 광선을 따른 거리로 할지입니다. `auto` 는 "
       "화면을 핀홀 면으로 나눈 경우에만 광선 깊이를 고릅니다. 학습 쪽의 "
       "--input-depth-is-ray-depth 를 비워 둔 때의 판단과 같습니다."),
    DE("Ob die gespeicherte Tiefe die Strecke entlang des Strahls ist statt "
       "einer z-Koordinate. `auto` nimmt Strahltiefe genau dann, wenn das Bild "
       "in Lochkamera-Flächen zerlegt wurde -- dieselbe Entscheidung, die "
       "--input-depth-is-ray-depth im Training ohne Angabe trifft."),
    FR("Si la profondeur enregistrée est la distance le long du rayon plutôt "
       "qu'une coordonnée z. `auto` choisit la profondeur radiale exactement "
       "quand l'image a été découpée en faces sténopé, le même choix que fait "
       "--input-depth-is-ray-depth à l'entraînement lorsqu'il n'est pas défini."),
    ES("Si la profundidad guardada es la distancia a lo largo del rayo en vez "
       "de una coordenada z. `auto` elige profundidad radial justo cuando el "
       "cuadro se dividió en caras estenopeicas, la misma decisión que toma "
       "--input-depth-is-ray-depth en el entrenamiento cuando no se indica."),
    PT("Se a profundidade guardada é a distância ao longo do raio em vez de "
       "uma coordenada z. `auto` escolhe profundidade radial exatamente quando "
       "o quadro foi dividido em faces estenopeicas, a mesma decisão que "
       "--input-depth-is-ray-depth toma no treino quando fica por definir."),
    IT("Se la profondità salvata è la distanza lungo il raggio anziché una "
       "coordinata z. `auto` sceglie la profondità radiale esattamente quando "
       "il fotogramma è stato diviso in facce stenopeiche, la stessa scelta di "
       "--input-depth-is-ray-depth nell'addestramento quando non è impostato."),
    NL("Of de opgeslagen diepte de afstand langs de straal is in plaats van "
       "een z-coördinaat. `auto` kiest straaldiepte precies wanneer het beeld "
       "in gaatjescamera-vlakken is gesplitst, dezelfde keuze die "
       "--input-depth-is-ray-depth bij de training maakt als die niet is gezet."),
    RU("Хранить ли глубину как расстояние вдоль луча, а не координату z. "
       "`auto` берёт лучевую глубину ровно тогда, когда кадр разбит на "
       "пинхол-грани — то же решение, что принимает --input-depth-is-ray-depth "
       "при обучении, если его не задать."),
    TR("Saklanan derinliğin z koordinatı yerine ışın boyunca uzaklık olup "
       "olmadığı. `auto`, kare iğne deliği yüzlerine bölündüğünde tam olarak "
       "ışın derinliğini seçer; eğitimde --input-depth-is-ray-depth "
       "belirtilmediğinde aynı karar verilir."));

SS_MSG(opt_split,
    EN("Whether to split a wide frame into several pinhole faces instead of "
       "undistorting it to one. `auto` splits a panorama always and a fisheye "
       "when one pinhole would keep less than three quarters of the frame."),
    JA("広い画角のフレームを、1 枚に歪み補正する代わりに複数のピンホール面へ"
       "分割するかです。`auto` はパノラマなら常に分割し、魚眼は 1 枚のピンホール"
       "で 4 分の 3 未満しか残らないときに分割します。"),
    ZH_HANS("是否把广角画面拆成多个针孔面，而不是矫正成一张。`auto` 对全景总是"
            "拆分，对鱼眼则在单张针孔保留不足四分之三画面时拆分。"),
    ZH_HANT("是否把廣角畫面拆成多個針孔面，而不是矯正成一張。`auto` 對全景總是"
            "拆分，對魚眼則在單張針孔保留不足四分之三畫面時拆分。"),
    KO("넓은 화각의 프레임을 한 장으로 왜곡 보정하는 대신 여러 핀홀 면으로 나눌지"
       "입니다. `auto` 는 파노라마는 항상 나누고, 어안은 핀홀 한 장이 화면의 4 분의 "
       "3 도 남기지 못할 때 나눕니다."),
    DE("Ob ein weites Bild in mehrere Lochkamera-Flächen zerlegt statt auf eine "
       "entzerrt wird. `auto` zerlegt ein Panorama immer und ein Fischauge "
       "dann, wenn eine Lochkamera weniger als drei Viertel des Bildes behält."),
    FR("Découper une image à large champ en plusieurs faces sténopé au lieu de "
       "la redresser en une seule. `auto` découpe toujours un panorama, et un "
       "fisheye quand un seul sténopé garderait moins des trois quarts."),
    ES("Si dividir un cuadro de campo amplio en varias caras estenopeicas en "
       "vez de enderezarlo en una. `auto` divide siempre un panorama, y un ojo "
       "de pez cuando un solo estenopo conservaría menos de tres cuartos."),
    PT("Se dividir um quadro de campo largo em várias faces estenopeicas em "
       "vez de o endireitar numa só. `auto` divide sempre um panorama, e uma "
       "olho-de-peixe quando um único estenopo guardaria menos de três quartos."),
    IT("Se dividere un fotogramma ad ampio campo in più facce stenopeiche "
       "invece di raddrizzarlo in una sola. `auto` divide sempre un panorama, e "
       "un fisheye quando un solo stenopeico terrebbe meno di tre quarti."),
    NL("Of een beeld met breed blikveld in meerdere gaatjescamera-vlakken wordt "
       "gesplitst in plaats van tot één rechtgetrokken. `auto` splitst een "
       "panorama altijd, en een fisheye als één gaatjescamera minder dan "
       "driekwart van het beeld overhoudt."),
    RU("Разбивать ли широкий кадр на несколько пинхол-граней вместо "
       "исправления в одну. `auto` всегда разбивает панораму, а «рыбий глаз» — "
       "когда одна пинхол-картинка сохранит меньше трёх четвертей кадра."),
    TR("Geniş bir karenin tek bir görüntüye düzeltilmesi yerine birkaç iğne "
       "deliği yüzüne bölünüp bölünmeyeceği. `auto` bir panoramayı hep böler, "
       "balık gözünü ise tek bir iğne deliği karenin dörtte üçünden azını "
       "koruyacaksa böler."));

SS_MSG(opt_depth_units,
    EN("What a stored depth value means. `relative` fills the 16 bits with the "
       "scene, dividing by its own 99.9th percentile, and is what the depth "
       "term wants -- it correlates log depths and ignores any scale. `mm` "
       "stores millimetres, which is readable but flattens everything past "
       "65.5 m."),
    JA("保存する深度値の意味です。`relative` は各画像の 99.9 パーセンタイルで割り、"
       "16 ビットをシーンに使い切ります。深度項は対数深度の相関を取るのでスケール"
       "を無視し、これが望ましい形です。`mm` はミリメートルで、読みやすい代わりに "
       "65.5 m より遠くが平らになります。"),
    ZH_HANS("保存的深度值代表什么。`relative` 按每张图自身的 99.9 百分位归一化，"
            "把 16 位全用在场景上；深度项计算对数深度的相关性、忽略尺度，正需要"
            "这种形式。`mm` 存毫米，可读但会把 65.5 m 以外压平。"),
    ZH_HANT("儲存的深度值代表什麼。`relative` 按每張圖自身的 99.9 百分位正規化，"
            "把 16 位元全用在場景上；深度項計算對數深度的相關性、忽略尺度，正需要"
            "這種形式。`mm` 存毫米，可讀但會把 65.5 m 以外壓平。"),
    KO("저장하는 깊이 값의 의미입니다. `relative` 는 각 이미지의 99.9 백분위수로 "
       "나눠 16 비트를 장면에 다 씁니다. 깊이 항은 로그 깊이의 상관을 보므로 배율을 "
       "무시하고, 이 형태를 원합니다. `mm` 는 밀리미터로 읽기 쉽지만 65.5 m 너머를 "
       "평평하게 만듭니다."),
    DE("Was ein gespeicherter Tiefenwert bedeutet. `relative` teilt durch das "
       "eigene 99,9-Perzentil und legt damit die 16 Bit auf die Szene -- was "
       "der Tiefenterm will, denn er korreliert Log-Tiefen und ignoriert jeden "
       "Maßstab. `mm` speichert Millimeter: lesbar, aber alles jenseits von "
       "65,5 m wird flach."),
    FR("Ce que vaut une valeur de profondeur enregistrée. `relative` divise par "
       "son propre 99,9e centile et met les 16 bits sur la scène -- ce que veut "
       "le terme de profondeur, qui corrèle des log-profondeurs et ignore "
       "l'échelle. `mm` enregistre des millimètres : lisible, mais tout "
       "au-delà de 65,5 m devient plat."),
    ES("Qué significa un valor de profundidad guardado. `relative` divide por "
       "su propio percentil 99,9 y pone los 16 bits en la escena, que es lo que "
       "quiere el término de profundidad: correlaciona log-profundidades e "
       "ignora la escala. `mm` guarda milímetros: legible, pero aplana todo más "
       "allá de 65,5 m."),
    PT("O que vale um valor de profundidade guardado. `relative` divide pelo "
       "seu próprio percentil 99,9 e põe os 16 bits na cena, que é o que o "
       "termo de profundidade quer: correlaciona log-profundidades e ignora a "
       "escala. `mm` guarda milímetros: legível, mas achata tudo para lá de "
       "65,5 m."),
    IT("Che cosa vale un valore di profondità salvato. `relative` divide per il "
       "proprio 99,9° percentile e mette i 16 bit sulla scena, che è ciò che "
       "vuole il termine di profondità: correla log-profondità e ignora la "
       "scala. `mm` salva millimetri: leggibile, ma appiattisce tutto oltre "
       "65,5 m."),
    NL("Wat een opgeslagen dieptewaarde betekent. `relative` deelt door het "
       "eigen 99,9e percentiel en legt de 16 bits op de scène -- wat de "
       "diepteterm wil, die log-diepten correleert en elke schaal negeert. "
       "`mm` slaat millimeters op: leesbaar, maar alles voorbij 65,5 m wordt "
       "vlak."),
    RU("Что означает сохранённое значение глубины. `relative` делит на "
       "собственный 99,9-й процентиль и отдаёт все 16 бит сцене — именно этого "
       "хочет член глубины: он коррелирует логарифмы и не смотрит на масштаб. "
       "`mm` хранит миллиметры: читаемо, но всё дальше 65,5 м становится "
       "плоским."),
    TR("Saklanan bir derinlik değerinin anlamı. `relative` kendi 99,9 "
       "yüzdebirliğine böler ve 16 biti sahneye ayırır -- derinlik teriminin "
       "istediği budur, çünkü logaritmik derinlikleri ilişkilendirir ve ölçeği "
       "yok sayar. `mm` milimetre saklar: okunaklıdır ama 65,5 m ötesini "
       "düzleştirir."));

SS_MSG(opt_image_gamut,
    EN("Colour primaries the dataset's images are in. Frames convert to sRGB "
       "before inference, which is what the model was trained on."),
    JA("データセット画像の色域。推論の前に sRGB へ変換します。モデルはそれで"
       "学習されています。"),
    ZH_HANS("数据集图像的色域。推理前转换为 sRGB，模型即以此训练。"),
    ZH_HANT("資料集影像的色域。推論前轉換為 sRGB，模型即以此訓練。"),
    KO("데이터셋 이미지의 색역. 추론 전에 sRGB 로 변환하며, 모델이 그것으로 "
       "학습되었습니다."),
    DE("Farbprimärvalenzen der Bilder des Datensatzes. Frames werden vor der "
       "Inferenz nach sRGB gewandelt, worauf das Modell trainiert wurde."),
    FR("Primaires de couleur des images du jeu de données. Les images passent "
       "en sRGB avant l'inférence, ce sur quoi le modèle a été entraîné."),
    ES("Primarios de color de las imágenes del conjunto. Los fotogramas pasan "
       "a sRGB antes de la inferencia, que es con lo que se entrenó el modelo."),
    PT("Primárias de cor das imagens do conjunto. Os quadros passam a sRGB "
       "antes da inferência, que é com o que o modelo foi treinado."),
    IT("Primarie di colore delle immagini del set. I fotogrammi passano a sRGB "
       "prima dell'inferenza, su cui il modello è stato addestrato."),
    NL("Kleurprimairen van de beelden in de dataset. Frames gaan naar sRGB voor "
       "de inferentie, waarop het model is getraind."),
    RU("Основные цвета изображений набора. Кадры переводятся в sRGB перед "
       "выводом — на этом обучалась модель."),
    TR("Veri kümesi görüntülerinin renk birincilleri. Kareler çıkarımdan önce "
       "sRGB'ye çevrilir; model bununla eğitildi."));

SS_MSG(opt_image_linear,
    EN("Treat the dataset's images as linear light rather than "
       "display-encoded."),
    JA("データセット画像を表示用エンコードではなくリニア光として扱います。"),
    ZH_HANS("将数据集图像视为线性光而非显示编码。"),
    ZH_HANT("將資料集影像視為線性光而非顯示編碼。"),
    KO("데이터셋 이미지를 디스플레이 인코딩이 아니라 선형 광으로 취급합니다."),
    DE("Bilder des Datensatzes als lineares Licht statt als anzeigecodiert "
       "behandeln."),
    FR("Traiter les images du jeu de données comme de la lumière linéaire "
       "plutôt qu'encodées pour l'affichage."),
    ES("Tratar las imágenes del conjunto como luz lineal en vez de codificadas "
       "para pantalla."),
    PT("Tratar as imagens do conjunto como luz linear em vez de codificadas "
       "para exibição."),
    IT("Trattare le immagini del set come luce lineare anziché codificate per "
       "lo schermo."),
    NL("Beelden van de dataset als lineair licht behandelen in plaats van "
       "weergavegecodeerd."),
    RU("Считать изображения набора линейным светом, а не экранно "
       "закодированными."),
    TR("Veri kümesi görüntülerini ekran kodlu değil, doğrusal ışık olarak ele "
       "al."));

SS_MSG(opt_overwrite,
    EN("Recompute maps that are already on disk. Without it a run continues "
       "where the last one stopped."),
    JA("すでにディスクにあるマップも作り直します。指定しなければ、前回の続きから"
       "実行します。"),
    ZH_HANS("重新计算磁盘上已有的图。不加则从上次中断处继续。"),
    ZH_HANT("重新計算磁碟上已有的圖。不加則從上次中斷處繼續。"),
    KO("이미 디스크에 있는 맵도 다시 계산합니다. 없으면 지난번에 멈춘 곳부터 "
       "이어서 실행합니다."),
    DE("Bereits vorhandene Karten neu berechnen. Ohne das setzt ein Lauf dort "
       "fort, wo der letzte aufgehört hat."),
    FR("Recalculer les cartes déjà présentes sur le disque. Sans cela, une "
       "exécution reprend là où la précédente s'est arrêtée."),
    ES("Recalcular los mapas que ya están en disco. Sin esto, una ejecución "
       "continúa donde se detuvo la anterior."),
    PT("Recalcular os mapas que já estão no disco. Sem isto, uma execução "
       "continua onde a anterior parou."),
    IT("Ricalcolare le mappe già presenti su disco. Senza, un'esecuzione "
       "riprende da dove si è fermata la precedente."),
    NL("Kaarten die al op schijf staan opnieuw berekenen. Zonder dit gaat een "
       "run verder waar de vorige stopte."),
    RU("Пересчитать карты, уже лежащие на диске. Без этого запуск продолжает с "
       "того места, где остановился предыдущий."),
    TR("Diskte zaten bulunan haritaları yeniden hesapla. Bu olmadan bir "
       "çalıştırma, öncekinin bıraktığı yerden sürer."));

SS_MSG(label_common,
    EN("Also:"), JA("その他:"), ZH_HANS("其他:"), ZH_HANT("其他:"), KO("그 밖에:"),
    DE("Außerdem:"), FR("Aussi :"), ES("Además:"), PT("Além disso:"),
    IT("Inoltre:"), NL("Verder:"), RU("Ещё:"), TR("Ayrıca:"));

SS_MSG(label_environment,
    EN("Environment:"), JA("環境変数:"), ZH_HANS("环境变量:"), ZH_HANT("環境變數:"),
    KO("환경 변수:"), DE("Umgebung:"), FR("Environnement :"), ES("Entorno:"),
    PT("Ambiente:"), IT("Ambiente:"), NL("Omgeving:"), RU("Переменные среды:"),
    TR("Ortam:"));

SS_MSG(err_no_dataset,
    EN("Name a dataset folder. `{0} --help` lists what it accepts."),
    JA("データセットのフォルダを指定してください。`{0} --help` に受け付ける形式が"
       "あります。"),
    ZH_HANS("请指定数据集文件夹。`{0} --help` 列出了它接受的格式。"),
    ZH_HANT("請指定資料集資料夾。`{0} --help` 列出了它接受的格式。"),
    KO("데이터셋 폴더를 지정하세요. `{0} --help` 에 받아들이는 형식이 있습니다."),
    DE("Bitte einen Datensatzordner angeben. `{0} --help` zeigt, was er "
       "annimmt."),
    FR("Indiquez un dossier de jeu de données. `{0} --help` liste ce qu'il "
       "accepte."),
    ES("Indica una carpeta de conjunto de datos. `{0} --help` enumera lo que "
       "acepta."),
    PT("Indique uma pasta de conjunto de dados. `{0} --help` lista o que "
       "aceita."),
    IT("Indica una cartella di insieme di dati. `{0} --help` elenca ciò che "
       "accetta."),
    NL("Geef een datasetmap op. `{0} --help` somt op wat hij aanneemt."),
    RU("Укажите папку набора данных. `{0} --help` перечисляет, что она "
       "принимает."),
    TR("Bir veri kümesi klasörü belirtin. `{0} --help` neyi kabul ettiğini "
       "listeler."));

SS_MSG(err_no_images_read,
    EN("Not one image could be read under '{0}'. Pass --image-dir if the "
       "dataset's images are somewhere else."),
    JA("「{0}」の下からは 1 枚も画像を読み取れませんでした。データセットの画像が"
       "別の場所にあるなら --image-dir で指定してください。"),
    ZH_HANS("在「{0}」下一张图像也读不到。如果数据集的图像在别处，"
            "请用 --image-dir 指定。"),
    ZH_HANT("在「{0}」下一張影像也讀不到。如果資料集的影像在別處，"
            "請用 --image-dir 指定。"),
    KO("'{0}' 아래에서 이미지를 한 장도 읽지 못했습니다. 데이터셋의 이미지가 "
       "다른 곳에 있으면 --image-dir 로 지정하세요."),
    DE("Unter '{0}' ließ sich kein einziges Bild lesen. Geben Sie --image-dir "
       "an, wenn die Bilder des Datensatzes woanders liegen."),
    FR("Aucune image n'a pu être lue sous « {0} ». Indiquez --image-dir si les "
       "images du jeu de données sont ailleurs."),
    ES("No se ha podido leer ninguna imagen bajo «{0}». Indica --image-dir si "
       "las imágenes del conjunto están en otro sitio."),
    PT("Não foi possível ler nenhuma imagem em \"{0}\". Informe --image-dir se "
       "as imagens do conjunto estiverem noutro lugar."),
    IT("Non è stato possibile leggere alcuna immagine sotto \"{0}\". Indica "
       "--image-dir se le immagini dell'insieme sono altrove."),
    NL("Onder '{0}' kon geen enkel beeld worden gelezen. Geef --image-dir op "
       "als de beelden van de dataset elders staan."),
    RU("В «{0}» не удалось прочитать ни одного изображения. Укажите "
       "--image-dir, если изображения набора лежат в другом месте."),
    TR("'{0}' altında hiçbir görüntü okunamadı. Veri kümesinin görüntüleri "
       "başka yerdeyse --image-dir ile belirtin."));

SS_MSG(log_dataset,
    EN("{0}: {1} images, {2} cameras"),
    JA("{0}: 画像 {1} 枚、カメラ {2} 台"),
    ZH_HANS("{0}: 图像 {1} 张, 相机 {2} 台"),
    ZH_HANT("{0}: 影像 {1} 張, 相機 {2} 台"),
    KO("{0}: 이미지 {1} 장, 카메라 {2} 대"),
    DE("{0}: Bilder {1}, Kameras {2}"),
    FR("{0} : images {1}, caméras {2}"),
    ES("{0}: imágenes {1}, cámaras {2}"),
    PT("{0}: imagens {1}, câmaras {2}"),
    IT("{0}: immagini {1}, fotocamere {2}"),
    NL("{0}: afbeeldingen {1}, camera's {2}"),
    RU("{0}: изображений {1}, камер {2}"),
    TR("{0}: görüntü {1}, kamera {2}"));

SS_MSG(log_camera_single,
    EN("camera {0}: {1}, undistorted to one {2}x{3} pinhole"),
    JA("カメラ {0}: {1}、1 枚の {2}x{3} ピンホールに歪み補正"),
    ZH_HANS("相机 {0}: {1}, 矫正为一张 {2}x{3} 针孔图"),
    ZH_HANT("相機 {0}: {1}, 矯正為一張 {2}x{3} 針孔圖"),
    KO("카메라 {0}: {1}, 한 장의 {2}x{3} 핀홀로 왜곡 보정"),
    DE("Kamera {0}: {1}, auf eine Lochkamera {2}x{3} entzerrt"),
    FR("caméra {0} : {1}, redressée en un sténopé {2}x{3}"),
    ES("cámara {0}: {1}, enderezada a un estenopo {2}x{3}"),
    PT("câmara {0}: {1}, endireitada para um estenopo {2}x{3}"),
    IT("fotocamera {0}: {1}, raddrizzata in uno stenopeico {2}x{3}"),
    NL("camera {0}: {1}, rechtgetrokken tot één gaatjescamera {2}x{3}"),
    RU("камера {0}: {1}, исправлена в одну пинхол-картинку {2}x{3}"),
    TR("kamera {0}: {1}, tek bir {2}x{3} iğne deliğine düzeltildi"));

SS_MSG(log_camera_split,
    EN("camera {0}: {1}, split into {2} pinhole faces of {3}x{4}"),
    JA("カメラ {0}: {1}、{3}x{4} のピンホール面 {2} 枚に分割"),
    ZH_HANS("相机 {0}: {1}, 拆成 {2} 个 {3}x{4} 的针孔面"),
    ZH_HANT("相機 {0}: {1}, 拆成 {2} 個 {3}x{4} 的針孔面"),
    KO("카메라 {0}: {1}, {3}x{4} 핀홀 면 {2} 개로 분할"),
    DE("Kamera {0}: {1}, in {2} Lochkamera-Flächen zu {3}x{4} zerlegt"),
    FR("caméra {0} : {1}, découpée en {2} faces sténopé de {3}x{4}"),
    ES("cámara {0}: {1}, dividida en {2} caras estenopeicas de {3}x{4}"),
    PT("câmara {0}: {1}, dividida em {2} faces estenopeicas de {3}x{4}"),
    IT("fotocamera {0}: {1}, divisa in {2} facce stenopeiche da {3}x{4}"),
    NL("camera {0}: {1}, gesplitst in {2} gaatjescamera-vlakken van {3}x{4}"),
    RU("камера {0}: {1}, разбита на {2} пинхол-граней {3}x{4}"),
    TR("kamera {0}: {1}, {3}x{4} boyutunda {2} iğne deliği yüzüne bölündü"));

SS_MSG(log_progress,
    EN("{0} / {1} images, {2} ms each, {3} left"),
    JA("{0} / {1} 枚、1 枚あたり {2} ms、残り {3}"),
    ZH_HANS("{0} / {1} 张, 每张 {2} ms, 剩余 {3}"),
    ZH_HANT("{0} / {1} 張, 每張 {2} ms, 剩餘 {3}"),
    KO("{0} / {1} 장, 장당 {2} ms, {3} 남음"),
    DE("{0} / {1} Bilder, {2} ms je Bild, {3} übrig"),
    FR("{0} / {1} images, {2} ms chacune, {3} restant"),
    ES("{0} / {1} imágenes, {2} ms cada una, quedan {3}"),
    PT("{0} / {1} imagens, {2} ms cada, faltam {3}"),
    IT("{0} / {1} immagini, {2} ms ciascuna, {3} rimanenti"),
    NL("{0} / {1} afbeeldingen, {2} ms per stuk, nog {3}"),
    RU("{0} / {1} изображений, по {2} мс, осталось {3}"),
    TR("{0} / {1} görüntü, her biri {2} ms, {3} kaldı"));

SS_MSG(log_done,
    EN("done: {0} written, {1} already there, in {2}"),
    JA("完了: {0} 枚を書き出し、{1} 枚は既存、所要 {2}"),
    ZH_HANS("完成: 写出 {0}, 已存在 {1}, 用时 {2}"),
    ZH_HANT("完成: 寫出 {0}, 已存在 {1}, 用時 {2}"),
    KO("완료: {0} 개 기록, {1} 개는 이미 있음, 소요 {2}"),
    DE("fertig: {0} geschrieben, {1} schon vorhanden, in {2}"),
    FR("terminé : {0} écrites, {1} déjà là, en {2}"),
    ES("listo: {0} escritas, {1} ya estaban, en {2}"),
    PT("concluído: {0} escritas, {1} já existiam, em {2}"),
    IT("fatto: {0} scritte, {1} già presenti, in {2}"),
    NL("klaar: {0} geschreven, {1} stonden er al, in {2}"),
    RU("готово: записано {0}, уже было {1}, за {2}"),
    TR("bitti: {0} yazıldı, {1} zaten vardı, süre {2}"));

SS_MSG(warn_fitted_camera,
    EN("camera {0} has a lens no distortion tier represents exactly; the maps "
       "are resampled through the fit and land within a pixel of the images"),
    JA("カメラ {0} のレンズは、どの歪みモデルでも厳密には表せません。マップは"
       "近似を通して再サンプルされ、画像とは 1 画素以内でずれます"),
    ZH_HANS("相机 {0} 的镜头没有任何畸变档位能精确表示；这些图会经拟合结果重采样，"
            "与图像相差在一个像素以内"),
    ZH_HANT("相機 {0} 的鏡頭沒有任何畸變檔位能精確表示；這些圖會經擬合結果重取樣，"
            "與影像相差在一個像素以內"),
    KO("카메라 {0} 의 렌즈는 어떤 왜곡 단계로도 정확히 표현되지 않습니다. 맵은 "
       "근사를 거쳐 다시 샘플링되며 이미지와 1 픽셀 이내로 어긋납니다"),
    DE("Kamera {0} hat ein Objektiv, das keine Verzeichnungsstufe exakt "
       "abbildet; die Karten werden über die Näherung neu abgetastet und "
       "liegen innerhalb eines Pixels zu den Bildern"),
    FR("la caméra {0} a un objectif qu'aucun palier de distorsion ne "
       "représente exactement ; les cartes sont rééchantillonnées via "
       "l'ajustement et tombent à un pixel près des images"),
    ES("la cámara {0} tiene un objetivo que ningún nivel de distorsión "
       "representa con exactitud; los mapas se remuestrean a través del ajuste "
       "y quedan a menos de un píxel de las imágenes"),
    PT("a câmara {0} tem uma objetiva que nenhum nível de distorção representa "
       "exatamente; os mapas são reamostrados através do ajuste e ficam a "
       "menos de um píxel das imagens"),
    IT("la fotocamera {0} ha un obiettivo che nessun livello di distorsione "
       "rappresenta esattamente; le mappe sono ricampionate attraverso "
       "l'approssimazione e cadono entro un pixel dalle immagini"),
    NL("camera {0} heeft een lens die geen enkele vervormingstrap precies "
       "weergeeft; de kaarten worden via de benadering herbemonsterd en vallen "
       "binnen een pixel van de afbeeldingen"),
    RU("у камеры {0} объектив, который ни один уровень дисторсии не описывает "
       "точно; карты пересэмплируются через приближение и ложатся в пределах "
       "пикселя от изображений"),
    TR("kamera {0}, hiçbir bozulma kademesinin tam olarak temsil etmediği bir "
       "mercek kullanıyor; haritalar uyum üzerinden yeniden örneklenir ve "
       "görüntülerin bir piksel yakınına düşer"));

}  // namespace geometry
}  // namespace msg
}  // namespace i18n
}  // namespace spirula

#include "i18n/EndCatalog.h"
