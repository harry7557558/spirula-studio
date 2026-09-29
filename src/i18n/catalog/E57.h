#pragma once

// What `spirula e57` says -- its --help and every line a run prints, which the
// GUI's E57 screen reads back through i18n::scan() -- and that screen's copy.
//
// Flag names and file names stay as they are in every language.

#include "i18n/BeginCatalog.h"

namespace spirula {
namespace i18n {
namespace msg {
namespace e57 {

// ===========================================================================
// Command line
// ===========================================================================

SS_MSG(tagline,
    EN("A training dataset from an E57 laser scan, without a reconstruction"),
    JA("E57 レーザースキャンから、再構成なしで学習用データセットを作ります"),
    ZH_HANS("无需重建，直接从 E57 激光扫描生成训练数据集"),
    ZH_HANT("無需重建，直接從 E57 雷射掃描產生訓練資料集"),
    KO("재구성 없이 E57 레이저 스캔으로 학습용 데이터셋을 만듭니다"),
    DE("Ein Trainingsdatensatz aus einem E57-Laserscan, ohne Rekonstruktion"),
    FR("Un jeu de données d'entraînement tiré d'un scan laser E57, sans "
       "reconstruction"),
    ES("Un conjunto de datos de entrenamiento a partir de un escaneo láser E57, "
       "sin reconstrucción"),
    PT("Um conjunto de dados de treino a partir de uma varredura a laser E57, "
       "sem reconstrução"),
    IT("Un set di dati di addestramento da una scansione laser E57, senza "
       "ricostruzione"),
    NL("Een trainingsdataset uit een E57-laserscan, zonder reconstructie"),
    RU("Набор данных для обучения из лазерного скана E57, без реконструкции"),
    TR("Yeniden oluşturma yapmadan, bir E57 lazer taramasından eğitim veri "
       "kümesi"));

SS_MSG(usage_about,
    EN("Writes the images the scan carries, their poses and a thinned copy of "
       "its point cloud as a dataset the trainer opens directly: images/, "
       "transforms.json and sparse_pc.ply. The folder defaults to one beside "
       "the scan, named after it."),
    JA("スキャンに含まれる画像とその姿勢、間引いた点群を、トレーナーがそのまま"
       "開けるデータセット（images/、transforms.json、sparse_pc.ply）として"
       "書き出します。フォルダを指定しなければ、スキャンの隣に同じ名前で作ります。"),
    ZH_HANS("把扫描中的图像、它们的位姿和精简后的点云写成训练器可直接打开的数据集："
            "images/、transforms.json 和 sparse_pc.ply。默认文件夹位于扫描文件旁，"
            "与其同名。"),
    ZH_HANT("把掃描中的影像、它們的位姿和精簡後的點雲寫成訓練器可直接開啟的資料集："
            "images/、transforms.json 和 sparse_pc.ply。預設資料夾位於掃描檔旁，"
            "與其同名。"),
    KO("스캔에 담긴 이미지와 그 자세, 솎아 낸 포인트 클라우드를 트레이너가 바로 "
       "여는 데이터셋(images/, transforms.json, sparse_pc.ply)으로 씁니다. "
       "폴더를 지정하지 않으면 스캔 옆에 같은 이름으로 만듭니다."),
    DE("Schreibt die Bilder des Scans, ihre Posen und eine ausgedünnte Kopie "
       "seiner Punktwolke als Datensatz, den der Trainer direkt öffnet: "
       "images/, transforms.json und sparse_pc.ply. Ohne Angabe liegt der "
       "Ordner neben dem Scan und trägt seinen Namen."),
    FR("Écrit les images du scan, leurs poses et une copie allégée de son nuage "
       "de points sous forme d'un jeu de données que l'entraînement ouvre "
       "directement : images/, transforms.json et sparse_pc.ply. Par défaut, le "
       "dossier est placé à côté du scan et porte son nom."),
    ES("Escribe las imágenes del escaneo, sus poses y una copia aligerada de su "
       "nube de puntos como un conjunto de datos que el entrenador abre "
       "directamente: images/, transforms.json y sparse_pc.ply. Por defecto, la "
       "carpeta queda junto al escaneo y lleva su nombre."),
    PT("Grava as imagens da varredura, suas poses e uma cópia reduzida da nuvem "
       "de pontos como um conjunto de dados que o treinador abre diretamente: "
       "images/, transforms.json e sparse_pc.ply. Por padrão, a pasta fica ao "
       "lado da varredura, com o mesmo nome."),
    IT("Scrive le immagini della scansione, le loro pose e una copia sfoltita "
       "della nuvola di punti come un set di dati che l'addestratore apre "
       "direttamente: images/, transforms.json e sparse_pc.ply. Per impostazione "
       "predefinita la cartella sta accanto alla scansione, con lo stesso nome."),
    NL("Schrijft de beelden van de scan, hun poses en een uitgedunde kopie van "
       "de puntenwolk als een dataset die de trainer direct opent: images/, "
       "transforms.json en sparse_pc.ply. Standaard komt de map naast de scan, "
       "met dezelfde naam."),
    RU("Записывает изображения скана, их позы и прореженную копию облака точек "
       "как набор данных, который тренажёр открывает сразу: images/, "
       "transforms.json и sparse_pc.ply. По умолчанию папка создаётся рядом со "
       "сканом и называется так же."),
    TR("Taramadaki görüntüleri, pozlarını ve nokta bulutunun seyreltilmiş bir "
       "kopyasını eğiticinin doğrudan açtığı bir veri kümesi olarak yazar: "
       "images/, transforms.json ve sparse_pc.ply. Varsayılan klasör taramanın "
       "yanında, onunla aynı adla oluşturulur."));

SS_MSG(head_options,
    EN("Options"), JA("オプション"), ZH_HANS("选项"), ZH_HANT("選項"),
    KO("옵션"), DE("Optionen"), FR("Options"), ES("Opciones"), PT("Opções"),
    IT("Opzioni"), NL("Opties"), RU("Параметры"), TR("Seçenekler"));

SS_MSG(opt_points,
    EN("Thin the point cloud to about this many seed points, one per voxel. "
       "`all` keeps every point; 0 writes no cloud. Default: {0}"),
    JA("点群をボクセルごとに 1 点へ間引き、初期点をおよそこの数にします。`all` "
       "なら全点を残し、0 なら点群を書き出しません。既定: {0}"),
    ZH_HANS("按体素把点云精简到约这么多个初始点，每个体素一个。`all` 表示保留"
            "全部点，0 表示不写点云。默认：{0}"),
    ZH_HANT("按體素把點雲精簡到約這麼多個初始點，每個體素一個。`all` 表示保留"
            "全部點，0 表示不寫點雲。預設：{0}"),
    KO("포인트 클라우드를 복셀마다 한 점씩 솎아 초기 점을 대략 이만큼으로 "
       "만듭니다. `all`이면 모든 점을 남기고, 0이면 클라우드를 쓰지 않습니다. "
       "기본값: {0}"),
    DE("Die Punktwolke auf etwa so viele Startpunkte ausdünnen, einer je Voxel. "
       "`all` behält alle Punkte, 0 schreibt keine Wolke. Standard: {0}"),
    FR("Alléger le nuage à environ ce nombre de points de départ, un par voxel. "
       "`all` garde tous les points ; 0 n'écrit aucun nuage. Par défaut : {0}"),
    ES("Aligerar la nube hasta unos tantos puntos iniciales, uno por vóxel. "
       "`all` conserva todos los puntos; 0 no escribe ninguna nube. Por "
       "defecto: {0}"),
    PT("Reduzir a nuvem a cerca de tantos pontos iniciais, um por voxel. `all` "
       "mantém todos os pontos; 0 não grava nuvem. Padrão: {0}"),
    IT("Sfoltire la nuvola a circa questo numero di punti iniziali, uno per "
       "voxel. `all` conserva tutti i punti; 0 non scrive alcuna nuvola. "
       "Predefinito: {0}"),
    NL("De puntenwolk uitdunnen tot ongeveer zoveel beginpunten, één per voxel. "
       "`all` houdt alle punten; 0 schrijft geen wolk. Standaard: {0}"),
    RU("Проредить облако примерно до стольких начальных точек, по одной на "
       "воксель. `all` сохраняет все точки, 0 -- облако не записывается. По "
       "умолчанию: {0}"),
    TR("Nokta bulutunu voksel başına bir nokta olacak şekilde yaklaşık bu kadar "
       "başlangıç noktasına seyrelt. `all` tüm noktaları tutar; 0 bulut yazmaz. "
       "Varsayılan: {0}"));

SS_MSG(opt_no_pinhole,
    EN("Leave out the pinhole images."),
    JA("ピンホール画像を含めません。"),
    ZH_HANS("不包含针孔图像。"),
    ZH_HANT("不包含針孔影像。"),
    KO("핀홀 이미지를 빼고 만듭니다."),
    DE("Die Lochkamerabilder weglassen."),
    FR("Laisser de côté les images sténopé."),
    ES("Omitir las imágenes estenopeicas."),
    PT("Deixar de fora as imagens pinhole."),
    IT("Escludere le immagini stenopeiche."),
    NL("De pinhole-beelden weglaten."),
    RU("Не включать изображения камеры-обскуры."),
    TR("İğne deliği görüntülerini dışarıda bırak."));

SS_MSG(opt_no_panoramas,
    EN("Leave out the spherical panoramas."),
    JA("球面パノラマを含めません。"),
    ZH_HANS("不包含球面全景图。"),
    ZH_HANT("不包含球面全景影像。"),
    KO("구면 파노라마를 빼고 만듭니다."),
    DE("Die sphärischen Panoramen weglassen."),
    FR("Laisser de côté les panoramas sphériques."),
    ES("Omitir las panorámicas esféricas."),
    PT("Deixar de fora os panoramas esféricos."),
    IT("Escludere i panorami sferici."),
    NL("De bolvormige panorama's weglaten."),
    RU("Не включать сферические панорамы."),
    TR("Küresel panoramaları dışarıda bırak."));

SS_MSG(opt_overwrite,
    EN("Write into a folder that is not empty. The dataset's own files are "
       "replaced; nothing else is removed."),
    JA("空でないフォルダにも書き込みます。データセットのファイルは置き換えますが、"
       "それ以外は削除しません。"),
    ZH_HANS("允许写入非空文件夹。数据集自己的文件会被替换，其他内容不会删除。"),
    ZH_HANT("允許寫入非空資料夾。資料集自己的檔案會被取代，其他內容不會刪除。"),
    KO("비어 있지 않은 폴더에도 씁니다. 데이터셋 자체의 파일은 바꾸고, 다른 것은 "
       "지우지 않습니다."),
    DE("In einen nicht leeren Ordner schreiben. Die Dateien des Datensatzes "
       "werden ersetzt, sonst wird nichts entfernt."),
    FR("Écrire dans un dossier qui n'est pas vide. Les fichiers du jeu de "
       "données sont remplacés ; rien d'autre n'est supprimé."),
    ES("Escribir en una carpeta que no está vacía. Los archivos del conjunto se "
       "reemplazan; no se borra nada más."),
    PT("Gravar numa pasta que não está vazia. Os arquivos do conjunto são "
       "substituídos; nada mais é removido."),
    IT("Scrivere in una cartella non vuota. I file del set di dati vengono "
       "sostituiti; nient'altro viene rimosso."),
    NL("In een map schrijven die niet leeg is. De bestanden van de dataset "
       "worden vervangen; verder wordt niets verwijderd."),
    RU("Писать в непустую папку. Файлы набора данных заменяются, остальное не "
       "удаляется."),
    TR("Boş olmayan bir klasöre yaz. Veri kümesinin kendi dosyaları değiştirilir; "
       "başka hiçbir şey silinmez."));

SS_MSG(opt_info,
    EN("List what the file holds and write nothing."),
    JA("ファイルの中身を一覧にするだけで、何も書き出しません。"),
    ZH_HANS("只列出文件内容，不写任何东西。"),
    ZH_HANT("只列出檔案內容，不寫任何東西。"),
    KO("파일 내용만 나열하고 아무것도 쓰지 않습니다."),
    DE("Auflisten, was die Datei enthält, und nichts schreiben."),
    FR("Lister le contenu du fichier sans rien écrire."),
    ES("Listar lo que contiene el archivo sin escribir nada."),
    PT("Listar o que o arquivo contém, sem gravar nada."),
    IT("Elencare il contenuto del file senza scrivere nulla."),
    NL("Tonen wat het bestand bevat en niets schrijven."),
    RU("Показать содержимое файла и ничего не записывать."),
    TR("Dosyanın içeriğini listele ve hiçbir şey yazma."));

SS_MSG(err_no_input,
    EN("Name an .e57 file. `{0} --help` describes the options."),
    JA(".e57 ファイルを指定してください。オプションは `{0} --help` にあります。"),
    ZH_HANS("请指定一个 .e57 文件。`{0} --help` 说明了各选项。"),
    ZH_HANT("請指定一個 .e57 檔案。`{0} --help` 說明了各選項。"),
    KO(".e57 파일을 지정하세요. 옵션은 `{0} --help`에 있습니다."),
    DE("Eine .e57-Datei angeben. `{0} --help` beschreibt die Optionen."),
    FR("Indiquez un fichier .e57. `{0} --help` décrit les options."),
    ES("Indique un archivo .e57. `{0} --help` describe las opciones."),
    PT("Indique um arquivo .e57. `{0} --help` descreve as opções."),
    IT("Indicare un file .e57. `{0} --help` descrive le opzioni."),
    NL("Geef een .e57-bestand op. `{0} --help` beschrijft de opties."),
    RU("Укажите файл .e57. Параметры описаны в `{0} --help`."),
    TR("Bir .e57 dosyası belirtin. Seçenekler `{0} --help` içinde."));

SS_MSG(err_unknown_option,
    EN("Unknown option: {0}"), JA("不明なオプション: {0}"),
    ZH_HANS("未知选项：{0}"), ZH_HANT("未知選項：{0}"), KO("알 수 없는 옵션: {0}"),
    DE("Unbekannte Option: {0}"), FR("Option inconnue : {0}"),
    ES("Opción desconocida: {0}"), PT("Opção desconhecida: {0}"),
    IT("Opzione sconosciuta: {0}"), NL("Onbekende optie: {0}"),
    RU("Неизвестный параметр: {0}"), TR("Bilinmeyen seçenek: {0}"));

SS_MSG(err_bad_number,
    EN("{0} takes a whole number, not '{1}'"),
    JA("{0} には整数を指定してください（'{1}' ではなく）"),
    ZH_HANS("{0} 需要整数，而不是“{1}”"),
    ZH_HANT("{0} 需要整數，而不是「{1}」"),
    KO("{0}에는 정수가 필요합니다('{1}'가 아니라)"),
    DE("{0} erwartet eine ganze Zahl, nicht '{1}'"),
    FR("{0} attend un nombre entier, pas « {1} »"),
    ES("{0} espera un número entero, no '{1}'"),
    PT("{0} espera um número inteiro, não '{1}'"),
    IT("{0} richiede un numero intero, non '{1}'"),
    NL("{0} verwacht een geheel getal, niet '{1}'"),
    RU("{0} ожидает целое число, а не «{1}»"),
    TR("{0} bir tam sayı bekler, '{1}' değil"));

SS_MSG(opt_no_depth,
    EN("Do not write depth and normal maps from the scan."),
    JA("スキャンから深度マップと法線マップを書き出しません。"),
    ZH_HANS("不从扫描写入深度图和法线图。"),
    ZH_HANT("不從掃描寫入深度圖和法線圖。"),
    KO("스캔에서 깊이 맵과 법선 맵을 쓰지 않습니다."),
    DE("Keine Tiefen- und Normalenkarten aus dem Scan schreiben."),
    FR("Ne pas écrire de cartes de profondeur et de normales tirées du scan."),
    ES("No escribir mapas de profundidad y normales del escaneo."),
    PT("Não gravar mapas de profundidade e normais da varredura."),
    IT("Non scrivere mappe di profondità e normali dalla scansione."),
    NL("Geen diepte- en normaalkaarten uit de scan schrijven."),
    RU("Не записывать карты глубины и нормалей из скана."),
    TR("Taramadan derinlik ve normal haritası yazma."));

// ===========================================================================
// What a run prints (the GUI scans progress_images / progress_points)
// ===========================================================================

SS_MSG(summary,
    EN("Scans: {0}   Points: {1}   Images: {2}"),
    JA("スキャン: {0}   点: {1}   画像: {2}"),
    ZH_HANS("扫描：{0}   点：{1}   图像：{2}"),
    ZH_HANT("掃描：{0}   點：{1}   影像：{2}"),
    KO("스캔: {0}   점: {1}   이미지: {2}"),
    DE("Scans: {0}   Punkte: {1}   Bilder: {2}"),
    FR("Scans : {0}   Points : {1}   Images : {2}"),
    ES("Escaneos: {0}   Puntos: {1}   Imágenes: {2}"),
    PT("Varreduras: {0}   Pontos: {1}   Imagens: {2}"),
    IT("Scansioni: {0}   Punti: {1}   Immagini: {2}"),
    NL("Scans: {0}   Punten: {1}   Beelden: {2}"),
    RU("Сканов: {0}   Точек: {1}   Изображений: {2}"),
    TR("Tarama: {0}   Nokta: {1}   Görüntü: {2}"));

SS_MSG(summary_images,
    EN("Pinhole images: {0}   Panoramas: {1}   Other: {2}"),
    JA("ピンホール画像: {0}   パノラマ: {1}   その他: {2}"),
    ZH_HANS("针孔图像：{0}   全景图：{1}   其他：{2}"),
    ZH_HANT("針孔影像：{0}   全景影像：{1}   其他：{2}"),
    KO("핀홀 이미지: {0}   파노라마: {1}   기타: {2}"),
    DE("Lochkamerabilder: {0}   Panoramen: {1}   Andere: {2}"),
    FR("Images sténopé : {0}   Panoramas : {1}   Autres : {2}"),
    ES("Imágenes estenopeicas: {0}   Panorámicas: {1}   Otras: {2}"),
    PT("Imagens pinhole: {0}   Panoramas: {1}   Outras: {2}"),
    IT("Immagini stenopeiche: {0}   Panorami: {1}   Altre: {2}"),
    NL("Pinhole-beelden: {0}   Panorama's: {1}   Overig: {2}"),
    RU("Изображений обскуры: {0}   Панорам: {1}   Прочих: {2}"),
    TR("İğne deliği görüntüsü: {0}   Panorama: {1}   Diğer: {2}"));

SS_MSG(info_scan,
    EN("Scan {0}: {1}   Points: {2}"),
    JA("スキャン {0}: {1}   点: {2}"),
    ZH_HANS("扫描 {0}：{1}   点：{2}"),
    ZH_HANT("掃描 {0}：{1}   點：{2}"),
    KO("스캔 {0}: {1}   점: {2}"),
    DE("Scan {0}: {1}   Punkte: {2}"),
    FR("Scan {0} : {1}   Points : {2}"),
    ES("Escaneo {0}: {1}   Puntos: {2}"),
    PT("Varredura {0}: {1}   Pontos: {2}"),
    IT("Scansione {0}: {1}   Punti: {2}"),
    NL("Scan {0}: {1}   Punten: {2}"),
    RU("Скан {0}: {1}   Точек: {2}"),
    TR("Tarama {0}: {1}   Nokta: {2}"));

SS_MSG(progress_images,
    EN("Writing images: {0}/{1}"),
    JA("画像を書き出し中: {0}/{1}"),
    ZH_HANS("正在写入图像：{0}/{1}"),
    ZH_HANT("正在寫入影像：{0}/{1}"),
    KO("이미지 쓰는 중: {0}/{1}"),
    DE("Bilder werden geschrieben: {0}/{1}"),
    FR("Écriture des images : {0}/{1}"),
    ES("Escribiendo imágenes: {0}/{1}"),
    PT("Gravando imagens: {0}/{1}"),
    IT("Scrittura delle immagini: {0}/{1}"),
    NL("Beelden schrijven: {0}/{1}"),
    RU("Запись изображений: {0}/{1}"),
    TR("Görüntüler yazılıyor: {0}/{1}"));

SS_MSG(progress_points,
    EN("Reading scans: {0}/{1}"),
    JA("スキャンを読み込み中: {0}/{1}"),
    ZH_HANS("正在读取扫描：{0}/{1}"),
    ZH_HANT("正在讀取掃描：{0}/{1}"),
    KO("스캔 읽는 중: {0}/{1}"),
    DE("Scans werden gelesen: {0}/{1}"),
    FR("Lecture des scans : {0}/{1}"),
    ES("Leyendo escaneos: {0}/{1}"),
    PT("Lendo varreduras: {0}/{1}"),
    IT("Lettura delle scansioni: {0}/{1}"),
    NL("Scans lezen: {0}/{1}"),
    RU("Чтение сканов: {0}/{1}"),
    TR("Taramalar okunuyor: {0}/{1}"));

SS_MSG(thinning,
    EN("Thinning the point cloud. Points read: {0}   Target: {1}"),
    JA("点群を間引いています。読み込んだ点: {0}   目標: {1}"),
    ZH_HANS("正在精简点云。已读取的点：{0}   目标：{1}"),
    ZH_HANT("正在精簡點雲。已讀取的點：{0}   目標：{1}"),
    KO("포인트 클라우드를 솎는 중. 읽은 점: {0}   목표: {1}"),
    DE("Punktwolke wird ausgedünnt. Gelesene Punkte: {0}   Ziel: {1}"),
    FR("Allègement du nuage de points. Points lus : {0}   Cible : {1}"),
    ES("Aligerando la nube de puntos. Puntos leídos: {0}   Objetivo: {1}"),
    PT("Reduzindo a nuvem de pontos. Pontos lidos: {0}   Alvo: {1}"),
    IT("Sfoltimento della nuvola di punti. Punti letti: {0}   Obiettivo: {1}"),
    NL("Puntenwolk uitdunnen. Gelezen punten: {0}   Doel: {1}"),
    RU("Прореживание облака точек. Прочитано точек: {0}   Цель: {1}"),
    TR("Nokta bulutu seyreltiliyor. Okunan nokta: {0}   Hedef: {1}"));

SS_MSG(seed_done,
    EN("Seed points: {0}   Voxel: {1} m"),
    JA("初期点: {0}   ボクセル: {1} m"),
    ZH_HANS("初始点：{0}   体素：{1} m"),
    ZH_HANT("初始點：{0}   體素：{1} m"),
    KO("초기 점: {0}   복셀: {1} m"),
    DE("Startpunkte: {0}   Voxel: {1} m"),
    FR("Points de départ : {0}   Voxel : {1} m"),
    ES("Puntos iniciales: {0}   Vóxel: {1} m"),
    PT("Pontos iniciais: {0}   Voxel: {1} m"),
    IT("Punti iniziali: {0}   Voxel: {1} m"),
    NL("Beginpunten: {0}   Voxel: {1} m"),
    RU("Начальных точек: {0}   Воксель: {1} м"),
    TR("Başlangıç noktası: {0}   Voksel: {1} m"));

SS_MSG(seed_all,
    EN("Seed points: {0}, every point of the scan"),
    JA("初期点: {0}（スキャンの全点）"),
    ZH_HANS("初始点：{0}（扫描中的全部点）"),
    ZH_HANT("初始點：{0}（掃描中的全部點）"),
    KO("초기 점: {0}(스캔의 모든 점)"),
    DE("Startpunkte: {0}, alle Punkte des Scans"),
    FR("Points de départ : {0}, tous les points du scan"),
    ES("Puntos iniciales: {0}, todos los puntos del escaneo"),
    PT("Pontos iniciais: {0}, todos os pontos da varredura"),
    IT("Punti iniziali: {0}, tutti i punti della scansione"),
    NL("Beginpunten: {0}, alle punten van de scan"),
    RU("Начальных точек: {0}, все точки скана"),
    TR("Başlangıç noktası: {0}, taramanın tüm noktaları"));

SS_MSG(progress_depth,
    EN("Writing depth maps: {0}/{1}"),
    JA("深度マップを書き出し中: {0}/{1}"),
    ZH_HANS("正在写入深度图：{0}/{1}"),
    ZH_HANT("正在寫入深度圖：{0}/{1}"),
    KO("깊이 맵 쓰는 중: {0}/{1}"),
    DE("Tiefenkarten werden geschrieben: {0}/{1}"),
    FR("Écriture des cartes de profondeur : {0}/{1}"),
    ES("Escribiendo mapas de profundidad: {0}/{1}"),
    PT("Gravando mapas de profundidade: {0}/{1}"),
    IT("Scrittura delle mappe di profondità: {0}/{1}"),
    NL("Dieptekaarten schrijven: {0}/{1}"),
    RU("Запись карт глубины: {0}/{1}"),
    TR("Derinlik haritaları yazılıyor: {0}/{1}"));

SS_MSG(check_running,
    EN("Checking that the photos line up with the scan..."),
    JA("写真とスキャンが一致しているか確認しています..."),
    ZH_HANS("正在检查照片与扫描是否对齐..."),
    ZH_HANT("正在檢查照片與掃描是否對齊..."),
    KO("사진이 스캔과 맞는지 확인하는 중..."),
    DE("Es wird geprüft, ob die Fotos zum Scan passen ..."),
    FR("Vérification de l'alignement des photos sur le scan..."),
    ES("Comprobando que las fotos coinciden con el escaneo..."),
    PT("Verificando se as fotos coincidem com a varredura..."),
    IT("Verifica che le foto corrispondano alla scansione..."),
    NL("Controleren of de foto's op de scan aansluiten..."),
    RU("Проверка совпадения фотографий со сканом..."),
    TR("Fotoğrafların taramayla örtüştüğü denetleniyor..."));

SS_MSG(check_no_color,
    EN("The scan has no colour, so its photos cannot be checked against it."),
    JA("スキャンに色がないため、写真と照合できません。"),
    ZH_HANS("扫描没有颜色，无法用它核对照片。"),
    ZH_HANT("掃描沒有顏色，無法用它核對照片。"),
    KO("스캔에 색이 없어 사진과 대조할 수 없습니다."),
    DE("Der Scan hat keine Farbe, daher lassen sich die Fotos nicht mit ihm "
       "abgleichen."),
    FR("Le scan n'a pas de couleur ; ses photos ne peuvent pas être vérifiées."),
    ES("El escaneo no tiene color, así que sus fotos no se pueden comprobar con "
       "él."),
    PT("A varredura não tem cor, então as fotos não podem ser conferidas com ela."),
    IT("La scansione non ha colore, quindi le foto non possono essere verificate."),
    NL("De scan heeft geen kleur, dus de foto's kunnen er niet mee worden "
       "vergeleken."),
    RU("Скан не содержит цвета, поэтому сверить с ним фотографии нельзя."),
    TR("Taramada renk yok; bu yüzden fotoğraflar onunla karşılaştırılamaz."));

SS_MSG(check_ok,
    EN("The photos line up with the scan. Colour correlation: {0}"),
    JA("写真はスキャンと一致しています。色の相関: {0}"),
    ZH_HANS("照片与扫描对齐。颜色相关性：{0}"),
    ZH_HANT("照片與掃描對齊。顏色相關性：{0}"),
    KO("사진이 스캔과 맞습니다. 색 상관계수: {0}"),
    DE("Die Fotos passen zum Scan. Farbkorrelation: {0}"),
    FR("Les photos sont alignées sur le scan. Corrélation des couleurs : {0}"),
    ES("Las fotos coinciden con el escaneo. Correlación de color: {0}"),
    PT("As fotos coincidem com a varredura. Correlação de cor: {0}"),
    IT("Le foto corrispondono alla scansione. Correlazione del colore: {0}"),
    NL("De foto's sluiten aan op de scan. Kleurcorrelatie: {0}"),
    RU("Фотографии совпадают со сканом. Корреляция цвета: {0}"),
    TR("Fotoğraflar taramayla örtüşüyor. Renk korelasyonu: {0}"));

SS_MSG(check_fixed_pinhole,
    EN("The pinhole images face another way than the E57 convention says; "
       "corrected. Colour correlation: {0} -> {1}"),
    JA("ピンホール画像の向きが E57 の規約と違っていたため、修正しました。色の相関: "
       "{0} -> {1}"),
    ZH_HANS("针孔图像的朝向与 E57 约定不同，已更正。颜色相关性：{0} -> {1}"),
    ZH_HANT("針孔影像的朝向與 E57 約定不同，已更正。顏色相關性：{0} -> {1}"),
    KO("핀홀 이미지의 방향이 E57 규약과 달라 바로잡았습니다. 색 상관계수: "
       "{0} -> {1}"),
    DE("Die Lochkamerabilder sind anders ausgerichtet, als die E57-Konvention "
       "sagt; korrigiert. Farbkorrelation: {0} -> {1}"),
    FR("Les images sténopé sont orientées autrement que ne le prévoit la "
       "convention E57 ; corrigé. Corrélation des couleurs : {0} -> {1}"),
    ES("Las imágenes estenopeicas están orientadas de otra forma que la "
       "convención E57; corregido. Correlación de color: {0} -> {1}"),
    PT("As imagens pinhole estão orientadas de outra forma que a convenção E57; "
       "corrigido. Correlação de cor: {0} -> {1}"),
    IT("Le immagini stenopeiche sono orientate diversamente dalla convenzione "
       "E57; corretto. Correlazione del colore: {0} -> {1}"),
    NL("De pinhole-beelden zijn anders gericht dan de E57-conventie zegt; "
       "gecorrigeerd. Kleurcorrelatie: {0} -> {1}"),
    RU("Изображения обскуры ориентированы не так, как требует соглашение E57; "
       "исправлено. Корреляция цвета: {0} -> {1}"),
    TR("İğne deliği görüntüleri E57 kuralından farklı yöne bakıyor; düzeltildi. "
       "Renk korelasyonu: {0} -> {1}"));

SS_MSG(check_fixed_panorama,
    EN("The panoramas face another way than the E57 convention says; corrected. "
       "Colour correlation: {0} -> {1}"),
    JA("パノラマの向きが E57 の規約と違っていたため、修正しました。色の相関: "
       "{0} -> {1}"),
    ZH_HANS("全景图的朝向与 E57 约定不同，已更正。颜色相关性：{0} -> {1}"),
    ZH_HANT("全景影像的朝向與 E57 約定不同，已更正。顏色相關性：{0} -> {1}"),
    KO("파노라마의 방향이 E57 규약과 달라 바로잡았습니다. 색 상관계수: {0} -> {1}"),
    DE("Die Panoramen sind anders ausgerichtet, als die E57-Konvention sagt; "
       "korrigiert. Farbkorrelation: {0} -> {1}"),
    FR("Les panoramas sont orientés autrement que ne le prévoit la convention "
       "E57 ; corrigé. Corrélation des couleurs : {0} -> {1}"),
    ES("Las panorámicas están orientadas de otra forma que la convención E57; "
       "corregido. Correlación de color: {0} -> {1}"),
    PT("Os panoramas estão orientados de outra forma que a convenção E57; "
       "corrigido. Correlação de cor: {0} -> {1}"),
    IT("I panorami sono orientati diversamente dalla convenzione E57; corretto. "
       "Correlazione del colore: {0} -> {1}"),
    NL("De panorama's zijn anders gericht dan de E57-conventie zegt; "
       "gecorrigeerd. Kleurcorrelatie: {0} -> {1}"),
    RU("Панорамы ориентированы не так, как требует соглашение E57; исправлено. "
       "Корреляция цвета: {0} -> {1}"),
    TR("Panoramalar E57 kuralından farklı yöne bakıyor; düzeltildi. Renk "
       "korelasyonu: {0} -> {1}"));

SS_MSG(check_unsure,
    EN("Could not confirm that the photos line up with the scan (colour "
       "correlation {0}). Look at the preview before training."),
    JA("写真がスキャンと一致しているか確認できませんでした（色の相関 {0}）。学習の"
       "前にプレビューを確認してください。"),
    ZH_HANS("无法确认照片与扫描对齐（颜色相关性 {0}）。训练前请先查看预览。"),
    ZH_HANT("無法確認照片與掃描對齊（顏色相關性 {0}）。訓練前請先查看預覽。"),
    KO("사진이 스캔과 맞는지 확인하지 못했습니다(색 상관계수 {0}). 학습 전에 "
       "미리보기를 확인하세요."),
    DE("Es ließ sich nicht bestätigen, dass die Fotos zum Scan passen "
       "(Farbkorrelation {0}). Vor dem Training die Vorschau ansehen."),
    FR("Impossible de confirmer l'alignement des photos sur le scan (corrélation "
       "des couleurs {0}). Vérifiez l'aperçu avant l'entraînement."),
    ES("No se pudo confirmar que las fotos coinciden con el escaneo (correlación "
       "de color {0}). Revise la vista previa antes de entrenar."),
    PT("Não foi possível confirmar que as fotos coincidem com a varredura "
       "(correlação de cor {0}). Confira a pré-visualização antes de treinar."),
    IT("Impossibile confermare che le foto corrispondano alla scansione "
       "(correlazione del colore {0}). Controllare l'anteprima prima "
       "dell'addestramento."),
    NL("Kon niet bevestigen dat de foto's op de scan aansluiten (kleurcorrelatie "
       "{0}). Bekijk de voorvertoning vóór het trainen."),
    RU("Не удалось подтвердить, что фотографии совпадают со сканом (корреляция "
       "цвета {0}). Проверьте предпросмотр перед обучением."),
    TR("Fotoğrafların taramayla örtüştüğü doğrulanamadı (renk korelasyonu {0}). "
       "Eğitimden önce önizlemeye bakın."));

SS_MSG(skip_no_pose,
    EN("Skipped {0}: the image has no pose."),
    JA("{0} を飛ばしました: 画像に姿勢がありません。"),
    ZH_HANS("已跳过 {0}：图像没有位姿。"),
    ZH_HANT("已略過 {0}：影像沒有位姿。"),
    KO("{0} 건너뜀: 이미지에 자세가 없습니다."),
    DE("{0} übersprungen: Das Bild hat keine Pose."),
    FR("{0} ignorée : l'image n'a pas de pose."),
    ES("{0} omitida: la imagen no tiene pose."),
    PT("{0} ignorada: a imagem não tem pose."),
    IT("{0} saltata: l'immagine non ha una posa."),
    NL("{0} overgeslagen: het beeld heeft geen pose."),
    RU("{0} пропущено: у изображения нет позы."),
    TR("{0} atlandı: görüntünün pozu yok."));

SS_MSG(skip_no_pixels,
    EN("Skipped {0}: the image carries neither a JPEG nor a PNG."),
    JA("{0} を飛ばしました: 画像に JPEG も PNG も含まれていません。"),
    ZH_HANS("已跳过 {0}：图像既没有 JPEG 也没有 PNG。"),
    ZH_HANT("已略過 {0}：影像既沒有 JPEG 也沒有 PNG。"),
    KO("{0} 건너뜀: 이미지에 JPEG도 PNG도 없습니다."),
    DE("{0} übersprungen: Das Bild enthält weder JPEG noch PNG."),
    FR("{0} ignorée : l'image ne contient ni JPEG ni PNG."),
    ES("{0} omitida: la imagen no contiene ni JPEG ni PNG."),
    PT("{0} ignorada: a imagem não contém JPEG nem PNG."),
    IT("{0} saltata: l'immagine non contiene né JPEG né PNG."),
    NL("{0} overgeslagen: het beeld bevat geen JPEG en geen PNG."),
    RU("{0} пропущено: в изображении нет ни JPEG, ни PNG."),
    TR("{0} atlandı: görüntüde ne JPEG ne de PNG var."));

SS_MSG(skip_cylindrical,
    EN("Skipped {0}: cylindrical images are not supported."),
    JA("{0} を飛ばしました: 円筒画像には対応していません。"),
    ZH_HANS("已跳过 {0}：不支持柱面图像。"),
    ZH_HANT("已略過 {0}：不支援柱面影像。"),
    KO("{0} 건너뜀: 원통형 이미지는 지원하지 않습니다."),
    DE("{0} übersprungen: Zylinderbilder werden nicht unterstützt."),
    FR("{0} ignorée : les images cylindriques ne sont pas prises en charge."),
    ES("{0} omitida: las imágenes cilíndricas no son compatibles."),
    PT("{0} ignorada: imagens cilíndricas não são suportadas."),
    IT("{0} saltata: le immagini cilindriche non sono supportate."),
    NL("{0} overgeslagen: cilindrische beelden worden niet ondersteund."),
    RU("{0} пропущено: цилиндрические изображения не поддерживаются."),
    TR("{0} atlandı: silindirik görüntüler desteklenmiyor."));

SS_MSG(skip_uncalibrated,
    EN("Skipped {0}: the image has no calibration."),
    JA("{0} を飛ばしました: 画像にキャリブレーションがありません。"),
    ZH_HANS("已跳过 {0}：图像没有标定。"),
    ZH_HANT("已略過 {0}：影像沒有校正。"),
    KO("{0} 건너뜀: 이미지에 보정 정보가 없습니다."),
    DE("{0} übersprungen: Das Bild hat keine Kalibrierung."),
    FR("{0} ignorée : l'image n'a pas d'étalonnage."),
    ES("{0} omitida: la imagen no tiene calibración."),
    PT("{0} ignorada: a imagem não tem calibração."),
    IT("{0} saltata: l'immagine non ha una calibrazione."),
    NL("{0} overgeslagen: het beeld heeft geen kalibratie."),
    RU("{0} пропущено: у изображения нет калибровки."),
    TR("{0} atlandı: görüntünün kalibrasyonu yok."));

SS_MSG(skip_bad_pinhole,
    EN("Skipped {0}: its pinhole calibration is incomplete."),
    JA("{0} を飛ばしました: ピンホールのキャリブレーションが不完全です。"),
    ZH_HANS("已跳过 {0}：针孔标定不完整。"),
    ZH_HANT("已略過 {0}：針孔校正不完整。"),
    KO("{0} 건너뜀: 핀홀 보정 정보가 불완전합니다."),
    DE("{0} übersprungen: Die Lochkamera-Kalibrierung ist unvollständig."),
    FR("{0} ignorée : son étalonnage sténopé est incomplet."),
    ES("{0} omitida: su calibración estenopeica está incompleta."),
    PT("{0} ignorada: a calibração pinhole está incompleta."),
    IT("{0} saltata: la calibrazione stenopeica è incompleta."),
    NL("{0} overgeslagen: de pinhole-kalibratie is onvolledig."),
    RU("{0} пропущено: калибровка обскуры неполная."),
    TR("{0} atlandı: iğne deliği kalibrasyonu eksik."));

SS_MSG(skip_partial_panorama,
    EN("Skipped {0}: the panorama covers {1} x {2} degrees, not the whole "
       "sphere."),
    JA("{0} を飛ばしました: パノラマの範囲が {1} x {2} 度で、全球ではありません。"),
    ZH_HANS("已跳过 {0}：全景图只覆盖 {1} x {2} 度，不是整个球面。"),
    ZH_HANT("已略過 {0}：全景影像只涵蓋 {1} x {2} 度，不是整個球面。"),
    KO("{0} 건너뜀: 파노라마가 {1} x {2}도만 덮고 있어 구 전체가 아닙니다."),
    DE("{0} übersprungen: Das Panorama deckt {1} x {2} Grad ab, nicht die "
       "ganze Kugel."),
    FR("{0} ignorée : le panorama couvre {1} x {2} degrés, pas la sphère "
       "entière."),
    ES("{0} omitida: la panorámica cubre {1} x {2} grados, no la esfera "
       "entera."),
    PT("{0} ignorada: o panorama cobre {1} x {2} graus, não a esfera inteira."),
    IT("{0} saltata: il panorama copre {1} x {2} gradi, non l'intera sfera."),
    NL("{0} overgeslagen: het panorama beslaat {1} x {2} graden, niet de hele "
       "bol."),
    RU("{0} пропущено: панорама охватывает {1} x {2} градусов, а не всю сферу."),
    TR("{0} atlandı: panorama tüm küreyi değil, {1} x {2} dereceyi kapsıyor."));

SS_MSG(err_no_images,
    EN("The scan has no image a dataset can use. Splats are trained on "
       "photographs; a point cloud alone is not enough."),
    JA("スキャンにデータセットで使える画像がありません。スプラットは写真から"
       "学習するので、点群だけでは足りません。"),
    ZH_HANS("扫描中没有数据集可用的图像。高斯点是用照片训练的，只有点云不够。"),
    ZH_HANT("掃描中沒有資料集可用的影像。高斯點是用照片訓練的，只有點雲不夠。"),
    KO("스캔에 데이터셋이 쓸 수 있는 이미지가 없습니다. 스플랫은 사진으로 "
       "학습하므로 포인트 클라우드만으로는 부족합니다."),
    DE("Der Scan enthält kein Bild, das ein Datensatz nutzen kann. Splats "
       "werden an Fotos trainiert; eine Punktwolke allein genügt nicht."),
    FR("Le scan ne contient aucune image utilisable par un jeu de données. Les "
       "splats s'entraînent sur des photos ; un nuage de points seul ne suffit "
       "pas."),
    ES("El escaneo no tiene ninguna imagen que un conjunto de datos pueda usar. "
       "Los splats se entrenan con fotografías; una nube de puntos sola no "
       "basta."),
    PT("A varredura não tem nenhuma imagem que um conjunto de dados possa usar. "
       "Os splats são treinados com fotografias; uma nuvem de pontos sozinha não "
       "basta."),
    IT("La scansione non ha immagini utilizzabili da un set di dati. Gli splat "
       "si addestrano su fotografie; una nuvola di punti da sola non basta."),
    NL("De scan bevat geen beeld dat een dataset kan gebruiken. Splats worden "
       "getraind op foto's; een puntenwolk alleen is niet genoeg."),
    RU("В скане нет изображений, пригодных для набора данных. Сплаты обучаются "
       "на фотографиях, одного облака точек недостаточно."),
    TR("Taramada bir veri kümesinin kullanabileceği görüntü yok. Splat'ler "
       "fotoğraflarla eğitilir; yalnızca nokta bulutu yetmez."));

SS_MSG(err_not_empty,
    EN("{0} is not empty. --overwrite writes into it anyway."),
    JA("{0} は空ではありません。--overwrite を付けると書き込みます。"),
    ZH_HANS("{0} 不是空的。加上 --overwrite 仍会写入。"),
    ZH_HANT("{0} 不是空的。加上 --overwrite 仍會寫入。"),
    KO("{0}이(가) 비어 있지 않습니다. --overwrite를 주면 그래도 씁니다."),
    DE("{0} ist nicht leer. Mit --overwrite wird trotzdem hineingeschrieben."),
    FR("{0} n'est pas vide. --overwrite y écrit quand même."),
    ES("{0} no está vacía. --overwrite escribe en ella de todos modos."),
    PT("{0} não está vazia. --overwrite grava nela mesmo assim."),
    IT("{0} non è vuota. --overwrite ci scrive comunque."),
    NL("{0} is niet leeg. --overwrite schrijft er toch in."),
    RU("{0} не пуста. С --overwrite запись всё равно выполняется."),
    TR("{0} boş değil. --overwrite yine de içine yazar."));

SS_MSG(done,
    EN("Dataset written: {0}   Images: {1}   Seed points: {2}"),
    JA("データセットを書き出しました: {0}   画像: {1}   初期点: {2}"),
    ZH_HANS("数据集已写入：{0}   图像：{1}   初始点：{2}"),
    ZH_HANT("資料集已寫入：{0}   影像：{1}   初始點：{2}"),
    KO("데이터셋을 썼습니다: {0}   이미지: {1}   초기 점: {2}"),
    DE("Datensatz geschrieben: {0}   Bilder: {1}   Startpunkte: {2}"),
    FR("Jeu de données écrit : {0}   Images : {1}   Points de départ : {2}"),
    ES("Conjunto de datos escrito: {0}   Imágenes: {1}   Puntos iniciales: {2}"),
    PT("Conjunto de dados gravado: {0}   Imagens: {1}   Pontos iniciais: {2}"),
    IT("Set di dati scritto: {0}   Immagini: {1}   Punti iniziali: {2}"),
    NL("Dataset geschreven: {0}   Beelden: {1}   Beginpunten: {2}"),
    RU("Набор данных записан: {0}   Изображений: {1}   Начальных точек: {2}"),
    TR("Veri kümesi yazıldı: {0}   Görüntü: {1}   Başlangıç noktası: {2}"));

SS_MSG(next_step,
    EN("Train on it with: {0}"),
    JA("学習するには: {0}"),
    ZH_HANS("用它训练：{0}"),
    ZH_HANT("用它訓練：{0}"),
    KO("학습하려면: {0}"),
    DE("Training darauf mit: {0}"),
    FR("Pour l'entraîner : {0}"),
    ES("Para entrenar con él: {0}"),
    PT("Para treinar com ele: {0}"),
    IT("Per addestrarlo: {0}"),
    NL("Trainen met: {0}"),
    RU("Обучение на нём: {0}"),
    TR("Bununla eğitmek için: {0}"));

// ===========================================================================
// The GUI screen
// ===========================================================================

SS_MSG(title,
    EN("Create Dataset from E57"),
    JA("E57 からデータセットを作成"),
    ZH_HANS("从 E57 创建数据集"),
    ZH_HANT("從 E57 建立資料集"),
    KO("E57로 데이터셋 만들기"),
    DE("Datensatz aus E57 erstellen"),
    FR("Créer un jeu de données à partir d'un E57"),
    ES("Crear un conjunto de datos a partir de un E57"),
    PT("Criar um conjunto de dados a partir de um E57"),
    IT("Crea un set di dati da un E57"),
    NL("Dataset maken uit E57"),
    RU("Создание набора данных из E57"),
    TR("E57'den veri kümesi oluştur"));

SS_MSG(intro,
    EN("A laser scanner that also takes photos has already placed every one of "
       "them. This writes them out as a dataset, with their poses and a thinned "
       "copy of the scan's point cloud to start the splats from, so training "
       "needs no reconstruction."),
    JA("写真も撮るレーザースキャナーは、すべての写真の位置をすでに求めています。"
       "ここでは写真をその姿勢と、スプラットの出発点になる間引いた点群と一緒に"
       "データセットとして書き出すので、再構成なしで学習できます。"),
    ZH_HANS("同时拍照的激光扫描仪已经确定了每张照片的位置。这里把照片连同位姿和"
            "精简后的点云（作为高斯点的起点）写成数据集，训练无需重建。"),
    ZH_HANT("同時拍照的雷射掃描儀已經確定了每張照片的位置。這裡把照片連同位姿和"
            "精簡後的點雲（作為高斯點的起點）寫成資料集，訓練無需重建。"),
    KO("사진도 찍는 레이저 스캐너는 이미 모든 사진의 위치를 알고 있습니다. 여기서는 "
       "사진을 자세, 그리고 스플랫의 출발점이 될 솎아 낸 포인트 클라우드와 함께 "
       "데이터셋으로 써서, 재구성 없이 학습할 수 있게 합니다."),
    DE("Ein Laserscanner, der auch fotografiert, hat jedes Foto bereits "
       "platziert. Hier werden sie mit ihren Posen und einer ausgedünnten Kopie "
       "der Punktwolke als Startpunkte für die Splats in einen Datensatz "
       "geschrieben, sodass das Training keine Rekonstruktion braucht."),
    FR("Un scanner laser qui prend aussi des photos les a déjà toutes placées. "
       "Elles sont écrites ici en jeu de données, avec leurs poses et une copie "
       "allégée du nuage de points pour démarrer les splats : l'entraînement se "
       "passe de reconstruction."),
    ES("Un escáner láser que también toma fotos ya ha situado cada una de "
       "ellas. Aquí se escriben como un conjunto de datos, con sus poses y una "
       "copia aligerada de la nube de puntos para iniciar los splats, así que el "
       "entrenamiento no necesita reconstrucción."),
    PT("Um scanner a laser que também tira fotos já posicionou cada uma delas. "
       "Aqui elas são gravadas como um conjunto de dados, com suas poses e uma "
       "cópia reduzida da nuvem de pontos para iniciar os splats, de modo que o "
       "treino dispensa reconstrução."),
    IT("Uno scanner laser che scatta anche foto le ha già posizionate tutte. "
       "Qui vengono scritte come set di dati, con le loro pose e una copia "
       "sfoltita della nuvola di punti da cui far partire gli splat, così "
       "l'addestramento non richiede ricostruzione."),
    NL("Een laserscanner die ook foto's maakt, heeft elke foto al geplaatst. "
       "Hier worden ze als dataset geschreven, met hun poses en een uitgedunde "
       "kopie van de puntenwolk om de splats mee te beginnen, zodat de training "
       "geen reconstructie nodig heeft."),
    RU("Лазерный сканер, который ещё и фотографирует, уже знает, где снят каждый "
       "кадр. Здесь снимки записываются как набор данных вместе с позами и "
       "прореженной копией облака точек, с которой начинаются сплаты, поэтому "
       "обучению не нужна реконструкция."),
    TR("Fotoğraf da çeken bir lazer tarayıcı her fotoğrafın yerini zaten "
       "belirlemiştir. Burada fotoğraflar pozlarıyla ve splat'lerin başlangıcı "
       "olacak seyreltilmiş nokta bulutuyla birlikte bir veri kümesine yazılır; "
       "eğitim yeniden oluşturma gerektirmez."));

SS_MSG(source,
    EN("E57 file"), JA("E57 ファイル"), ZH_HANS("E57 文件"), ZH_HANT("E57 檔案"),
    KO("E57 파일"), DE("E57-Datei"), FR("Fichier E57"), ES("Archivo E57"),
    PT("Arquivo E57"), IT("File E57"), NL("E57-bestand"), RU("Файл E57"),
    TR("E57 dosyası"));

SS_MSG(pick_source,
    EN("Choose an E57 File"), JA("E57 ファイルを選択"), ZH_HANS("选择 E57 文件"),
    ZH_HANT("選擇 E57 檔案"), KO("E57 파일 선택"), DE("E57-Datei wählen"),
    FR("Choisir un fichier E57"), ES("Elegir un archivo E57"),
    PT("Escolher um arquivo E57"), IT("Scegli un file E57"),
    NL("Kies een E57-bestand"), RU("Выберите файл E57"), TR("Bir E57 dosyası seç"));

SS_MSG(output,
    EN("Dataset folder"), JA("データセットのフォルダ"), ZH_HANS("数据集文件夹"),
    ZH_HANT("資料集資料夾"), KO("데이터셋 폴더"), DE("Datensatzordner"),
    FR("Dossier du jeu de données"), ES("Carpeta del conjunto de datos"),
    PT("Pasta do conjunto de dados"), IT("Cartella del set di dati"),
    NL("Datasetmap"), RU("Папка набора данных"), TR("Veri kümesi klasörü"));

SS_MSG(pick_output,
    EN("Choose the Dataset Folder"), JA("データセットのフォルダを選択"),
    ZH_HANS("选择数据集文件夹"), ZH_HANT("選擇資料集資料夾"),
    KO("데이터셋 폴더 선택"), DE("Datensatzordner wählen"),
    FR("Choisir le dossier du jeu de données"),
    ES("Elegir la carpeta del conjunto de datos"),
    PT("Escolher a pasta do conjunto de dados"),
    IT("Scegli la cartella del set di dati"), NL("Kies de datasetmap"),
    RU("Выберите папку набора данных"), TR("Veri kümesi klasörünü seç"));

SS_MSG(output_not_empty,
    EN("This folder is not empty. The dataset's own files in it will be "
       "replaced; nothing else is removed."),
    JA("このフォルダは空ではありません。中のデータセットのファイルは置き換わりますが、"
       "それ以外は削除しません。"),
    ZH_HANS("此文件夹不是空的。其中数据集自己的文件会被替换，其他内容不会删除。"),
    ZH_HANT("此資料夾不是空的。其中資料集自己的檔案會被取代，其他內容不會刪除。"),
    KO("이 폴더는 비어 있지 않습니다. 안의 데이터셋 파일은 바뀌지만 다른 것은 "
       "지우지 않습니다."),
    DE("Dieser Ordner ist nicht leer. Die Dateien des Datensatzes darin werden "
       "ersetzt, sonst wird nichts entfernt."),
    FR("Ce dossier n'est pas vide. Les fichiers du jeu de données qu'il contient "
       "seront remplacés ; rien d'autre n'est supprimé."),
    ES("Esta carpeta no está vacía. Los archivos del conjunto que contiene se "
       "reemplazarán; no se borra nada más."),
    PT("Esta pasta não está vazia. Os arquivos do conjunto nela serão "
       "substituídos; nada mais é removido."),
    IT("Questa cartella non è vuota. I file del set di dati al suo interno "
       "verranno sostituiti; nient'altro viene rimosso."),
    NL("Deze map is niet leeg. De bestanden van de dataset erin worden "
       "vervangen; verder wordt niets verwijderd."),
    RU("Эта папка не пуста. Файлы набора данных в ней будут заменены, остальное "
       "не удаляется."),
    TR("Bu klasör boş değil. İçindeki veri kümesi dosyaları değiştirilecek; "
       "başka hiçbir şey silinmez."));

SS_MSG(read_failed,
    EN("This file could not be read: {0}"),
    JA("このファイルを読み込めませんでした: {0}"),
    ZH_HANS("无法读取此文件：{0}"),
    ZH_HANT("無法讀取此檔案：{0}"),
    KO("이 파일을 읽을 수 없습니다: {0}"),
    DE("Diese Datei konnte nicht gelesen werden: {0}"),
    FR("Impossible de lire ce fichier : {0}"),
    ES("No se pudo leer este archivo: {0}"),
    PT("Não foi possível ler este arquivo: {0}"),
    IT("Impossibile leggere questo file: {0}"),
    NL("Dit bestand kon niet worden gelezen: {0}"),
    RU("Не удалось прочитать файл: {0}"),
    TR("Bu dosya okunamadı: {0}"));

SS_MSG(points,
    EN("Seed points"), JA("初期点"), ZH_HANS("初始点"), ZH_HANT("初始點"),
    KO("초기 점"), DE("Startpunkte"), FR("Points de départ"),
    ES("Puntos iniciales"), PT("Pontos iniciais"), IT("Punti iniziali"),
    NL("Beginpunten"), RU("Начальные точки"), TR("Başlangıç noktaları"));

SS_MSG(points_help,
    EN("The scan's point cloud is thinned to about this many points, one per "
       "voxel, so the splats start evenly over everything the scanner saw "
       "rather than crowded around each station. The trainer's own maximum "
       "still applies. 0 writes no cloud."),
    JA("スキャンの点群をボクセルごとに 1 点へ間引いて、およそこの数にします。"
       "スプラットは各設置点のまわりに偏らず、スキャナーが見た範囲全体から均等に"
       "始まります。トレーナー側の上限はそのまま効きます。0 なら点群を書き出し"
       "ません。"),
    ZH_HANS("把扫描点云按体素精简到约这么多个点（每个体素一个），让高斯点均匀地"
            "从扫描仪看到的所有地方开始，而不是挤在每个测站周围。训练器自身的上限"
            "仍然有效。0 表示不写点云。"),
    ZH_HANT("把掃描點雲按體素精簡到約這麼多個點（每個體素一個），讓高斯點均勻地"
            "從掃描儀看到的所有地方開始，而不是擠在每個測站周圍。訓練器自身的上限"
            "仍然有效。0 表示不寫點雲。"),
    KO("스캔 포인트 클라우드를 복셀마다 한 점씩 솎아 대략 이만큼으로 만듭니다. "
       "그래서 스플랫이 각 설치 지점 주변에 몰리지 않고 스캐너가 본 모든 곳에서 "
       "고르게 시작합니다. 트레이너 자체의 최대값은 그대로 적용됩니다. 0이면 "
       "클라우드를 쓰지 않습니다."),
    DE("Die Punktwolke des Scans wird auf etwa so viele Punkte ausgedünnt, "
       "einer je Voxel, damit die Splats gleichmäßig überall beginnen, wo der "
       "Scanner hingesehen hat, statt sich um jeden Standpunkt zu drängen. Das "
       "Maximum des Trainers gilt weiterhin. 0 schreibt keine Wolke."),
    FR("Le nuage de points du scan est allégé à environ ce nombre de points, un "
       "par voxel, pour que les splats partent uniformément de tout ce que le "
       "scanner a vu plutôt que de s'entasser autour de chaque station. Le "
       "maximum de l'entraînement reste appliqué. 0 n'écrit aucun nuage."),
    ES("La nube de puntos del escaneo se aligera hasta unos tantos puntos, uno "
       "por vóxel, para que los splats empiecen de forma uniforme en todo lo que "
       "vio el escáner en vez de amontonarse alrededor de cada estación. El "
       "máximo del entrenador sigue vigente. 0 no escribe ninguna nube."),
    PT("A nuvem de pontos da varredura é reduzida a cerca de tantos pontos, um "
       "por voxel, para que os splats comecem de forma uniforme em tudo o que o "
       "scanner viu, em vez de se amontoarem em volta de cada estação. O máximo "
       "do treinador continua valendo. 0 não grava nuvem."),
    IT("La nuvola di punti della scansione viene sfoltita a circa questo numero "
       "di punti, uno per voxel, così gli splat partono in modo uniforme da "
       "tutto ciò che lo scanner ha visto invece di ammassarsi attorno a ogni "
       "stazione. Il massimo dell'addestratore resta valido. 0 non scrive "
       "alcuna nuvola."),
    NL("De puntenwolk van de scan wordt uitgedund tot ongeveer zoveel punten, "
       "één per voxel, zodat de splats gelijkmatig beginnen overal waar de "
       "scanner keek in plaats van rond elke opstelling samen te klonteren. Het "
       "maximum van de trainer blijft gelden. 0 schrijft geen wolk."),
    RU("Облако точек скана прореживается примерно до стольких точек, по одной на "
       "воксель, чтобы сплаты начинались равномерно везде, куда смотрел сканер, "
       "а не скапливались вокруг каждой станции. Собственный максимум тренажёра "
       "по-прежнему действует. 0 -- облако не записывается."),
    TR("Taramanın nokta bulutu voksel başına bir nokta olacak şekilde yaklaşık "
       "bu kadar noktaya seyreltilir; böylece splat'ler her istasyonun çevresinde "
       "yığılmak yerine tarayıcının gördüğü her yerden eşit başlar. Eğiticinin "
       "kendi üst sınırı yine geçerlidir. 0 bulut yazmaz."));

SS_MSG(use_pinhole,
    EN("Pinhole images"), JA("ピンホール画像"), ZH_HANS("针孔图像"),
    ZH_HANT("針孔影像"), KO("핀홀 이미지"), DE("Lochkamerabilder"),
    FR("Images sténopé"), ES("Imágenes estenopeicas"), PT("Imagens pinhole"),
    IT("Immagini stenopeiche"), NL("Pinhole-beelden"),
    RU("Изображения обскуры"), TR("İğne deliği görüntüleri"));

SS_MSG(use_pinhole_help,
    EN("Photos from the scanner's camera, often six per station that together "
       "cover the sphere."),
    JA("スキャナーのカメラで撮った写真です。設置点ごとに 6 枚で全球を覆うことが"
       "よくあります。"),
    ZH_HANS("扫描仪相机拍的照片，常常每个测站六张，合起来覆盖整个球面。"),
    ZH_HANT("掃描儀相機拍的照片，常常每個測站六張，合起來涵蓋整個球面。"),
    KO("스캐너 카메라로 찍은 사진으로, 설치 지점마다 여섯 장이 모여 구 전체를 덮는 "
       "경우가 많습니다."),
    DE("Fotos der Scannerkamera, oft sechs je Standpunkt, die zusammen die "
       "ganze Kugel abdecken."),
    FR("Photos de la caméra du scanner, souvent six par station qui couvrent "
       "ensemble toute la sphère."),
    ES("Fotos de la cámara del escáner, a menudo seis por estación que juntas "
       "cubren toda la esfera."),
    PT("Fotos da câmera do scanner, muitas vezes seis por estação que juntas "
       "cobrem a esfera inteira."),
    IT("Foto della fotocamera dello scanner, spesso sei per stazione che "
       "insieme coprono l'intera sfera."),
    NL("Foto's van de scannercamera, vaak zes per opstelling die samen de hele "
       "bol bedekken."),
    RU("Снимки камеры сканера, часто по шесть на станцию, вместе покрывающие "
       "всю сферу."),
    TR("Tarayıcının kamerasıyla çekilen fotoğraflar; çoğu zaman istasyon başına "
       "altı tane, birlikte tüm küreyi kaplar."));

SS_MSG(use_panoramas,
    EN("Panoramas"), JA("パノラマ"), ZH_HANS("全景图"), ZH_HANT("全景影像"),
    KO("파노라마"), DE("Panoramen"), FR("Panoramas"), ES("Panorámicas"),
    PT("Panoramas"), IT("Panorami"), NL("Panorama's"), RU("Панорамы"),
    TR("Panoramalar"));

SS_MSG(use_panoramas_help,
    EN("Spherical 360-degree images. Training splits each into six cube faces "
       "unless that option is turned off."),
    JA("360 度の球面画像です。学習ではその設定を切らない限り、各画像を 6 つの"
       "キューブ面に分割します。"),
    ZH_HANS("360 度球面图像。除非关闭该选项，训练时会把每张拆成六个立方体面。"),
    ZH_HANT("360 度球面影像。除非關閉該選項，訓練時會把每張拆成六個立方體面。"),
    KO("360도 구면 이미지입니다. 그 옵션을 끄지 않는 한, 학습은 각각을 여섯 개의 "
       "큐브 면으로 나눕니다."),
    DE("Sphärische 360-Grad-Bilder. Das Training teilt jedes in sechs "
       "Würfelflächen, sofern diese Option nicht abgeschaltet ist."),
    FR("Images sphériques à 360 degrés. L'entraînement découpe chacune en six "
       "faces de cube, sauf si cette option est désactivée."),
    ES("Imágenes esféricas de 360 grados. El entrenamiento divide cada una en "
       "seis caras de cubo, salvo que esa opción esté desactivada."),
    PT("Imagens esféricas de 360 graus. O treino divide cada uma em seis faces "
       "de cubo, a menos que essa opção esteja desligada."),
    IT("Immagini sferiche a 360 gradi. L'addestramento divide ciascuna in sei "
       "facce di cubo, a meno che l'opzione sia disattivata."),
    NL("Bolvormige 360-gradenbeelden. De training splitst elk in zes "
       "kubusvlakken, tenzij die optie uit staat."),
    RU("Сферические изображения на 360 градусов. Обучение делит каждое на шесть "
       "граней куба, если эта настройка не выключена."),
    TR("360 derecelik küresel görüntüler. Bu seçenek kapatılmadıkça eğitim her "
       "birini altı küp yüzüne böler."));

SS_MSG(use_all_points,
    EN("Use every point"), JA("すべての点を使う"), ZH_HANS("使用全部点"),
    ZH_HANT("使用全部點"), KO("모든 점 사용"), DE("Alle Punkte verwenden"),
    FR("Utiliser tous les points"), ES("Usar todos los puntos"),
    PT("Usar todos os pontos"), IT("Usa tutti i punti"),
    NL("Alle punten gebruiken"), RU("Использовать все точки"),
    TR("Tüm noktaları kullan"));

SS_MSG(use_all_points_help,
    EN("Seed with the scan's whole point cloud instead of a thinned copy. A "
       "large scan makes a large file, and the trainer still keeps at most its "
       "Maximum splats of them, drawn at random, so raise that too."),
    JA("間引いたコピーではなく、スキャンの点群全体を初期点にします。大きなスキャン"
       "ではファイルも大きくなります。トレーナーはスプラット数の上限までしか残さず、"
       "残りを無作為に捨てるので、その値も上げてください。"),
    ZH_HANS("用扫描的完整点云而不是精简副本作为初始点。大扫描会生成大文件，而且训练"
            "器最多只随机保留“泼溅数量上限”个点，所以也请调高该值。"),
    ZH_HANT("用掃描的完整點雲而不是精簡副本作為初始點。大掃描會產生大檔案，而且訓練"
            "器最多只隨機保留「潑濺數量上限」個點，所以也請調高該值。"),
    KO("솎아 낸 사본 대신 스캔의 전체 포인트 클라우드를 초기 점으로 씁니다. 큰 "
       "스캔은 큰 파일이 되고, 트레이너는 최대 스플랫 수까지만 무작위로 남기므로 "
       "그 값도 올리세요."),
    DE("Mit der ganzen Punktwolke des Scans statt einer ausgedünnten Kopie "
       "beginnen. Ein großer Scan ergibt eine große Datei, und der Trainer behält "
       "davon höchstens seine Höchstzahl der Splats, zufällig gezogen – diese "
       "also ebenfalls erhöhen."),
    FR("Partir du nuage de points complet du scan plutôt que d'une copie "
       "allégée. Un grand scan donne un grand fichier, et l'entraînement n'en "
       "garde au plus que son nombre maximal de splats, tirés au hasard : "
       "augmentez-le aussi."),
    ES("Empezar con la nube de puntos completa del escaneo en vez de una copia "
       "aligerada. Un escaneo grande produce un archivo grande, y el entrenador "
       "solo conserva como mucho su número máximo de splats, elegidos al azar, "
       "así que súbalo también."),
    PT("Começar com a nuvem de pontos completa da varredura em vez de uma cópia "
       "reduzida. Uma varredura grande gera um arquivo grande, e o treinador "
       "mantém no máximo o seu número máximo de splats, sorteados, então "
       "aumente-o também."),
    IT("Partire dall'intera nuvola di punti della scansione invece che da una "
       "copia sfoltita. Una scansione grande produce un file grande, e "
       "l'addestratore ne tiene al massimo il suo numero massimo di splat, "
       "estratti a caso: aumentalo anche."),
    NL("Beginnen met de volledige puntenwolk van de scan in plaats van een "
       "uitgedunde kopie. Een grote scan geeft een groot bestand, en de trainer "
       "houdt er hoogstens zijn maximum aantal splats van over, willekeurig "
       "gekozen, dus verhoog dat ook."),
    RU("Начинать с полного облака точек скана, а не с прореженной копии. "
       "Большой скан даёт большой файл, а тренажёр оставит из него не больше "
       "своего максимума сплатов, выбранных случайно, так что увеличьте и его."),
    TR("İnceltilmiş bir kopya yerine taramanın tüm nokta bulutuyla başla. Büyük "
       "bir tarama büyük bir dosya üretir ve eğitici bunlardan en fazla splat "
       "sayısı kadarını rastgele tutar; onu da artırın."));

SS_MSG(depth_maps,
    EN("Depth and normal maps from the scan"),
    JA("スキャンから深度マップと法線マップ"),
    ZH_HANS("由扫描生成深度图和法线图"),
    ZH_HANT("由掃描產生深度圖和法線圖"),
    KO("스캔으로 깊이 맵과 법선 맵 만들기"),
    DE("Tiefen- und Normalenkarten aus dem Scan"),
    FR("Cartes de profondeur et de normales tirées du scan"),
    ES("Mapas de profundidad y normales del escaneo"),
    PT("Mapas de profundidade e normais da varredura"),
    IT("Mappe di profondità e normali dalla scansione"),
    NL("Diepte- en normaalkaarten uit de scan"),
    RU("Карты глубины и нормалей из скана"),
    TR("Taramadan derinlik ve normal haritaları"));

SS_MSG(depth_maps_help,
    EN("Renders what the laser measured behind each pixel into depths/ and "
       "normals/, where the trainer's depth and normal supervision read it. "
       "Pixels the scan did not reach, the sky among them, are left without."),
    JA("レーザーが各画素の奥で測った距離を depths/ と normals/ に書き出します。"
       "トレーナーの深度・法線の教師信号はここから読みます。スキャンが届かなかった"
       "画素（空など）には何も入れません。"),
    ZH_HANS("把激光在每个像素后方测得的结果渲染到 depths/ 和 normals/，训练器的"
            "深度与法线监督从这里读取。扫描未覆盖的像素（包括天空）留空。"),
    ZH_HANT("把雷射在每個像素後方測得的結果渲染到 depths/ 和 normals/，訓練器的"
            "深度與法線監督從這裡讀取。掃描未涵蓋的像素（包括天空）留空。"),
    KO("레이저가 각 픽셀 뒤에서 잰 값을 depths/와 normals/에 그려 넣습니다. "
       "트레이너의 깊이·법선 지도 학습이 여기서 읽습니다. 스캔이 닿지 않은 "
       "픽셀(하늘 등)은 비워 둡니다."),
    DE("Rendert, was der Laser hinter jedem Pixel gemessen hat, nach depths/ "
       "und normals/, wo die Tiefen- und Normalenüberwachung des Trainers liest. "
       "Pixel, die der Scan nicht erreicht hat, darunter der Himmel, bleiben leer."),
    FR("Rend ce que le laser a mesuré derrière chaque pixel dans depths/ et "
       "normals/, où la supervision de profondeur et de normales de "
       "l'entraînement le lit. Les pixels que le scan n'a pas atteints, dont le "
       "ciel, restent vides."),
    ES("Renderiza lo que el láser midió detrás de cada píxel en depths/ y "
       "normals/, de donde leen la supervisión de profundidad y de normales del "
       "entrenador. Los píxeles que el escaneo no alcanzó, entre ellos el cielo, "
       "quedan vacíos."),
    PT("Renderiza o que o laser mediu atrás de cada pixel em depths/ e normals/, "
       "de onde a supervisão de profundidade e de normais do treinador lê. Os "
       "pixels que a varredura não alcançou, entre eles o céu, ficam vazios."),
    IT("Rende ciò che il laser ha misurato dietro ogni pixel in depths/ e "
       "normals/, da cui leggono la supervisione di profondità e normali "
       "dell'addestratore. I pixel che la scansione non ha raggiunto, cielo "
       "compreso, restano vuoti."),
    NL("Rendert wat de laser achter elke pixel mat naar depths/ en normals/, "
       "waar de diepte- en normaalsupervisie van de trainer het leest. Pixels "
       "die de scan niet bereikte, waaronder de lucht, blijven leeg."),
    RU("Записывает в depths/ и normals/ то, что лазер измерил за каждым "
       "пикселем; оттуда их читает контроль глубины и нормалей тренажёра. "
       "Пиксели, до которых скан не дошёл, в том числе небо, остаются пустыми."),
    TR("Lazerin her pikselin ardında ölçtüğünü depths/ ve normals/ içine işler; "
       "eğiticinin derinlik ve normal denetimi buradan okur. Taramanın "
       "ulaşmadığı pikseller, gökyüzü dahil, boş bırakılır."));

SS_MSG(preview_frames,
    EN("Extracting the photos to try the mask on..."),
    JA("マスクを試すために写真を取り出しています..."),
    ZH_HANS("正在提取照片以试用遮罩..."),
    ZH_HANT("正在擷取相片以試用遮罩..."),
    KO("마스크를 시험할 사진을 꺼내는 중..."),
    DE("Fotos zum Ausprobieren der Maske werden entpackt ..."),
    FR("Extraction des photos pour essayer le masque..."),
    ES("Extrayendo las fotos para probar la máscara..."),
    PT("Extraindo as fotos para testar a máscara..."),
    IT("Estrazione delle foto per provare la maschera..."),
    NL("Foto's uitpakken om het masker te proberen..."),
    RU("Извлечение фотографий для пробы маски..."),
    TR("Maskeyi denemek için fotoğraflar çıkarılıyor..."));

SS_MSG(preview_loading,
    EN("Reading the scan for the preview..."),
    JA("プレビュー用にスキャンを読み込んでいます..."),
    ZH_HANS("正在读取扫描以生成预览..."),
    ZH_HANT("正在讀取掃描以產生預覽..."),
    KO("미리보기를 위해 스캔을 읽는 중..."),
    DE("Scan wird für die Vorschau gelesen ..."),
    FR("Lecture du scan pour l'aperçu..."),
    ES("Leyendo el escaneo para la vista previa..."),
    PT("Lendo a varredura para a pré-visualização..."),
    IT("Lettura della scansione per l'anteprima..."),
    NL("Scan lezen voor de voorvertoning..."),
    RU("Чтение скана для предпросмотра..."),
    TR("Önizleme için tarama okunuyor..."));

// The step of the run that reads the scans' points and checks the photos
// against them, in the dataset screen's row of steps.
SS_MSG(step_scans,
    EN("Scans"), JA("スキャン"), ZH_HANS("扫描"), ZH_HANT("掃描"), KO("스캔"),
    DE("Scans"), FR("Scans"), ES("Escaneos"), PT("Varreduras"),
    IT("Scansioni"), NL("Scans"), RU("Сканы"), TR("Taramalar"));

SS_MSG(finished,
    EN("Dataset created: {0}"), JA("データセットを作成しました: {0}"),
    ZH_HANS("数据集已创建：{0}"), ZH_HANT("資料集已建立：{0}"),
    KO("데이터셋을 만들었습니다: {0}"), DE("Datensatz erstellt: {0}"),
    FR("Jeu de données créé : {0}"), ES("Conjunto de datos creado: {0}"),
    PT("Conjunto de dados criado: {0}"), IT("Set di dati creato: {0}"),
    NL("Dataset gemaakt: {0}"), RU("Набор данных создан: {0}"),
    TR("Veri kümesi oluşturuldu: {0}"));

SS_MSG(failed,
    EN("Creating the dataset failed."), JA("データセットを作成できませんでした。"),
    ZH_HANS("创建数据集失败。"), ZH_HANT("建立資料集失敗。"),
    KO("데이터셋을 만들지 못했습니다."), DE("Der Datensatz konnte nicht erstellt werden."),
    FR("La création du jeu de données a échoué."),
    ES("No se pudo crear el conjunto de datos."),
    PT("Não foi possível criar o conjunto de dados."),
    IT("Creazione del set di dati non riuscita."),
    NL("De dataset kon niet worden gemaakt."),
    RU("Не удалось создать набор данных."), TR("Veri kümesi oluşturulamadı."));

SS_MSG(not_masked,
    EN("The dataset was written to {0}, but not masked. It trains as it is, "
       "without masks."),
    JA("データセットは {0} に書き出しましたが、マスクはかけていません。このままマスク"
       "なしで学習できます。"),
    ZH_HANS("数据集已写入 {0}，但未加遮罩。它可以按原样在没有遮罩的情况下训练。"),
    ZH_HANT("資料集已寫入 {0}，但未加遮罩。它可以照原樣在沒有遮罩的情況下訓練。"),
    KO("데이터셋은 {0}에 썼지만 마스킹하지 않았습니다. 마스크 없이 그대로 학습할 수 "
       "있습니다."),
    DE("Der Datensatz wurde nach {0} geschrieben, aber nicht maskiert. Er lässt "
       "sich so, ohne Masken, trainieren."),
    FR("Le jeu de données a été écrit dans {0}, mais pas masqué. Il s'entraîne "
       "tel quel, sans masques."),
    ES("El conjunto de datos se escribió en {0}, pero sin enmascarar. Se puede "
       "entrenar tal cual, sin máscaras."),
    PT("O conjunto de dados foi gravado em {0}, mas não foi mascarado. Ele pode "
       "ser treinado como está, sem máscaras."),
    IT("Il set di dati è stato scritto in {0}, ma non mascherato. Si può "
       "addestrare così com'è, senza maschere."),
    NL("De dataset is naar {0} geschreven, maar niet gemaskeerd. Hij traint "
       "zoals hij is, zonder maskers."),
    RU("Набор данных записан в {0}, но без масок. Его можно обучать как есть."),
    TR("Veri kümesi {0} konumuna yazıldı ancak maskelenmedi. Olduğu gibi, "
       "maskesiz eğitilebilir."));

SS_MSG(no_source,
    EN("choose an E57 file first"), JA("先に E57 ファイルを選んでください"),
    ZH_HANS("请先选择 E57 文件"), ZH_HANT("請先選擇 E57 檔案"),
    KO("먼저 E57 파일을 고르세요"), DE("zuerst eine E57-Datei wählen"),
    FR("choisissez d'abord un fichier E57"), ES("elija primero un archivo E57"),
    PT("escolha primeiro um arquivo E57"), IT("scegli prima un file E57"),
    NL("kies eerst een E57-bestand"), RU("сначала выберите файл E57"),
    TR("önce bir E57 dosyası seçin"));

}  // namespace e57
}  // namespace msg
}  // namespace i18n
}  // namespace spirula

#include "i18n/EndCatalog.h"
