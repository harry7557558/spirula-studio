#pragma once

// What `spirula sam --help` and `spirula sam extract --help` say.
//
// The flag names, their value syntax and the words they accept (`video`,
// `subject`, `background`) are identifiers and stay as they are in every
// language -- they are what the reader types. What is translated is the
// sentence beside each one, and the headings above them.
//
// `spirula sam`'s detection table is machine-readable output and is not here:
// a script reads it, and a script has no language.

#include "i18n/BeginCatalog.h"

namespace spirula {
namespace i18n {
namespace msg {
namespace samhelp {

SS_MSG(tagline,
    EN("SAM 2 / SAM 3 segmentation and tracking on Vulkan"),
    JA("Vulkan 上で動く SAM 2 / SAM 3 のセグメンテーションと追跡"),
    ZH_HANS("在 Vulkan 上运行的 SAM 2 / SAM 3 分割与跟踪"),
    ZH_HANT("在 Vulkan 上執行的 SAM 2 / SAM 3 分割與追蹤"),
    KO("Vulkan 위에서 도는 SAM 2 / SAM 3 분할과 추적"),
    DE("SAM 2 / SAM 3: Segmentierung und Verfolgung auf Vulkan"),
    FR("Segmentation et suivi SAM 2 / SAM 3 sur Vulkan"),
    ES("Segmentación y seguimiento SAM 2 / SAM 3 sobre Vulkan"),
    PT("Segmentação e rastreamento SAM 2 / SAM 3 sobre Vulkan"),
    IT("Segmentazione e tracciamento SAM 2 / SAM 3 su Vulkan"),
    NL("SAM 2 / SAM 3-segmentatie en -volging op Vulkan"),
    RU("Сегментация и слежение SAM 2 / SAM 3 на Vulkan"),
    TR("Vulkan üzerinde SAM 2 / SAM 3 bölütleme ve izleme"));

SS_MSG(cmd_devices,
    EN("List Vulkan devices and whether each meets the baseline."),
    JA("Vulkan デバイスの一覧と、それぞれが必要条件を満たすかを表示します。"),
    ZH_HANS("列出 Vulkan 设备，并说明每个是否达到基本要求。"),
    ZH_HANT("列出 Vulkan 裝置，並說明每個是否達到基本要求。"),
    KO("Vulkan 장치를 나열하고 각각이 기본 요건을 만족하는지 보입니다."),
    DE("Vulkan-Geräte auflisten und ob jedes die Mindestanforderungen erfüllt."),
    FR("Lister les périphériques Vulkan et dire si chacun atteint le minimum."),
    ES("Listar los dispositivos Vulkan y si cada uno cumple el mínimo."),
    PT("Listar os dispositivos Vulkan e se cada um cumpre o mínimo."),
    IT("Elencare i dispositivi Vulkan e se ciascuno soddisfa il minimo."),
    NL("Vulkan-apparaten opsommen en of elk aan de ondergrens voldoet."),
    RU("Перечислить устройства Vulkan и указать, отвечает ли каждое требованиям."),
    TR("Vulkan aygıtlarını listele ve her birinin taban gereksinimi karşılayıp "
       "karşılamadığını söyle."));

// ---- segment ----

SS_MSG(seg_text,
    EN("concept prompt (all matching instances)"),
    JA("概念のプロンプト（一致するインスタンスすべて）"),
    ZH_HANS("概念提示词（匹配到的全部实例）"),
    ZH_HANT("概念提示詞（匹配到的全部實例）"),
    KO("개념 프롬프트(맞아떨어지는 모든 인스턴스)"),
    DE("Begriffsprompt (alle passenden Instanzen)"),
    FR("consigne de concept (toutes les instances correspondantes)"),
    ES("indicación de concepto (todas las instancias que coincidan)"),
    PT("comando de conceito (todas as instâncias que correspondam)"),
    IT("prompt di concetto (tutte le istanze corrispondenti)"),
    NL("conceptprompt (alle overeenkomende exemplaren)"),
    RU("понятийный запрос (все подходящие экземпляры)"),
    TR("kavram istemi (uyan tüm örnekler)"));

SS_MSG(seg_box,
    EN("exemplar box; repeatable"),
    JA("見本となる矩形。複数回指定できます"),
    ZH_HANS("样例框；可重复给出"),
    ZH_HANT("樣例框；可重複給出"),
    KO("본보기 상자. 여러 번 줄 수 있습니다"),
    DE("Beispielkasten; wiederholbar"),
    FR("boîte exemple ; répétable"),
    ES("caja de ejemplo; repetible"),
    PT("caixa de exemplo; repetível"),
    IT("riquadro d'esempio; ripetibile"),
    NL("voorbeeldkader; herhaalbaar"),
    RU("образцовая рамка; можно повторять"),
    TR("örnek kutu; yinelenebilir"));

SS_MSG(seg_neg_box,
    EN("negative exemplar box; repeatable"),
    JA("負例となる矩形。複数回指定できます"),
    ZH_HANS("反例框；可重复给出"),
    ZH_HANT("反例框；可重複給出"),
    KO("반례 상자. 여러 번 줄 수 있습니다"),
    DE("negativer Beispielkasten; wiederholbar"),
    FR("boîte contre-exemple ; répétable"),
    ES("caja de contraejemplo; repetible"),
    PT("caixa de contraexemplo; repetível"),
    IT("riquadro controesempio; ripetibile"),
    NL("tegenvoorbeeldkader; herhaalbaar"),
    RU("отрицательная образцовая рамка; можно повторять"),
    TR("olumsuz örnek kutu; yinelenebilir"));

SS_MSG(seg_point,
    EN("positive click; repeatable (visual prompt)"),
    JA("正のクリック。複数回指定できます（視覚プロンプト）"),
    ZH_HANS("正向点击；可重复给出（视觉提示）"),
    ZH_HANT("正向點選；可重複給出（視覺提示）"),
    KO("긍정 클릭. 여러 번 줄 수 있습니다(시각 프롬프트)"),
    DE("positiver Klick; wiederholbar (visueller Prompt)"),
    FR("clic positif ; répétable (consigne visuelle)"),
    ES("clic positivo; repetible (indicación visual)"),
    PT("clique positivo; repetível (comando visual)"),
    IT("clic positivo; ripetibile (prompt visivo)"),
    NL("positieve klik; herhaalbaar (visuele prompt)"),
    RU("положительный щелчок; можно повторять (визуальный запрос)"),
    TR("olumlu tıklama; yinelenebilir (görsel istem)"));

SS_MSG(seg_neg_point,
    EN("negative click; repeatable"),
    JA("負のクリック。複数回指定できます"),
    ZH_HANS("负向点击；可重复给出"),
    ZH_HANT("負向點選；可重複給出"),
    KO("부정 클릭. 여러 번 줄 수 있습니다"),
    DE("negativer Klick; wiederholbar"),
    FR("clic négatif ; répétable"),
    ES("clic negativo; repetible"),
    PT("clique negativo; repetível"),
    IT("clic negativo; ripetibile"),
    NL("negatieve klik; herhaalbaar"),
    RU("отрицательный щелчок; можно повторять"),
    TR("olumsuz tıklama; yinelenebilir"));

SS_MSG(seg_prompt_box,
    EN("box prompt for the visual path"),
    JA("視覚経路に与える矩形プロンプト"),
    ZH_HANS("视觉路径的框提示"),
    ZH_HANT("視覺路徑的框提示"),
    KO("시각 경로에 주는 상자 프롬프트"),
    DE("Kastenprompt für den visuellen Pfad"),
    FR("consigne par boîte pour la voie visuelle"),
    ES("indicación por caja para la vía visual"),
    PT("comando por caixa para a via visual"),
    IT("prompt a riquadro per il percorso visivo"),
    NL("kaderprompt voor het visuele pad"),
    RU("рамочный запрос для визуального пути"),
    TR("görsel yol için kutu istemi"));

SS_MSG(seg_multimask,
    EN("return the three ambiguity masks"),
    JA("あいまい性に対応する 3 つのマスクを返します"),
    ZH_HANS("返回三张歧义掩码"),
    ZH_HANT("回傳三張歧義遮罩"),
    KO("모호성에 대응하는 마스크 세 장을 돌려줍니다"),
    DE("die drei Mehrdeutigkeitsmasken zurückgeben"),
    FR("renvoyer les trois masques d'ambiguïté"),
    ES("devolver las tres máscaras de ambigüedad"),
    PT("devolver as três máscaras de ambiguidade"),
    IT("restituire le tre maschere di ambiguità"),
    NL("de drie dubbelzinnigheidsmaskers teruggeven"),
    RU("вернуть три маски неоднозначности"),
    TR("üç belirsizlik maskesini döndür"));

SS_MSG(seg_threshold,
    EN("detection score threshold"),
    JA("検出スコアのしきい値"),
    ZH_HANS("检测得分阈值"),
    ZH_HANT("偵測得分閾值"),
    KO("검출 점수 임계값"),
    DE("Schwelle der Erkennungsbewertung"),
    FR("seuil du score de détection"),
    ES("umbral de la puntuación de detección"),
    PT("limiar da pontuação de detecção"),
    IT("soglia del punteggio di rilevamento"),
    NL("drempel van de detectiescore"),
    RU("порог оценки обнаружения"),
    TR("bulma puanı eşiği"));

SS_MSG(seg_nms,
    EN("NMS IoU threshold"),
    JA("NMS の IoU しきい値"),
    ZH_HANS("NMS 的 IoU 阈值"),
    ZH_HANT("NMS 的 IoU 閾值"),
    KO("NMS 의 IoU 임계값"),
    DE("IoU-Schwelle der NMS"),
    FR("seuil d'IoU de la NMS"),
    ES("umbral de IoU de la NMS"),
    PT("limiar de IoU da NMS"),
    IT("soglia di IoU della NMS"),
    NL("IoU-drempel van de NMS"),
    RU("порог IoU для NMS"),
    TR("NMS IoU eşiği"));

SS_MSG(seg_out,
    EN("write mask PNGs and an overlay"),
    JA("マスクの PNG とオーバーレイを書き出します"),
    ZH_HANS("写出掩码 PNG 与一张叠加图"),
    ZH_HANT("寫出遮罩 PNG 與一張疊加圖"),
    KO("마스크 PNG 와 오버레이를 씁니다"),
    DE("Masken-PNGs und eine Überlagerung schreiben"),
    FR("écrire les PNG de masque et une superposition"),
    ES("escribir los PNG de máscara y una superposición"),
    PT("escrever os PNG de máscara e uma sobreposição"),
    IT("scrivere i PNG delle maschere e una sovrapposizione"),
    NL("masker-PNG's en een overlay schrijven"),
    RU("записать PNG масок и наложение"),
    TR("maske PNG'lerini ve bir bindirmeyi yaz"));

// ---- track ----

SS_MSG(trk_text,
    EN("detect and track matching instances; semicolon-separated for several "
       "concepts"),
    JA("一致するインスタンスを検出して追跡します。複数の概念はセミコロン区切り"),
    ZH_HANS("检测并跟踪匹配到的实例；多个概念用分号分隔"),
    ZH_HANT("偵測並追蹤匹配到的實例；多個概念用分號分隔"),
    KO("맞아떨어지는 인스턴스를 검출해 추적합니다. 개념이 여럿이면 세미콜론으로 "
       "나눕니다"),
    DE("passende Instanzen erkennen und verfolgen; mehrere Begriffe durch "
       "Semikolon getrennt"),
    FR("détecter et suivre les instances correspondantes ; plusieurs concepts "
       "séparés par des points-virgules"),
    ES("detectar y seguir las instancias que coincidan; varios conceptos "
       "separados por punto y coma"),
    PT("detectar e rastrear as instâncias que correspondam; vários conceitos "
       "separados por ponto e vírgula"),
    IT("rilevare e tracciare le istanze corrispondenti; più concetti separati da "
       "punto e virgola"),
    NL("overeenkomende exemplaren opsporen en volgen; meerdere concepten "
       "gescheiden door puntkomma's"),
    RU("находить и отслеживать подходящие экземпляры; несколько понятий "
       "разделяются точкой с запятой"),
    TR("uyan örnekleri bul ve izle; birden çok kavram noktalı virgülle ayrılır"));

SS_MSG(trk_neg_text,
    EN("concepts to KEEP even where --text matches"),
    JA("--text に一致しても残す概念"),
    ZH_HANS("即使与 --text 匹配也要保留的概念"),
    ZH_HANT("即使與 --text 匹配也要保留的概念"),
    KO("--text 에 맞더라도 남길 개념"),
    DE("Begriffe, die BEHALTEN werden, auch wo --text zutrifft"),
    FR("concepts à GARDER même là où --text correspond"),
    ES("conceptos que se CONSERVAN aunque --text coincida"),
    PT("conceitos a MANTER mesmo onde --text corresponde"),
    IT("concetti da TENERE anche dove --text corrisponde"),
    NL("concepten die BEHOUDEN blijven, ook waar --text matcht"),
    RU("понятия, которые СОХРАНЯЮТСЯ, даже если под --text они подходят"),
    TR("--text uysa bile KORUNACAK kavramlar"));

SS_MSG(trk_point,
    EN("click on an object to track; repeatable"),
    JA("追跡する対象をクリックします。複数回指定できます"),
    ZH_HANS("点击要跟踪的对象；可重复给出"),
    ZH_HANT("點選要追蹤的對象；可重複給出"),
    KO("추적할 대상을 클릭합니다. 여러 번 줄 수 있습니다"),
    DE("auf ein zu verfolgendes Objekt klicken; wiederholbar"),
    FR("cliquer sur un objet à suivre ; répétable"),
    ES("clic en un objeto que seguir; repetible"),
    PT("clique num objeto a rastrear; repetível"),
    IT("clic su un oggetto da tracciare; ripetibile"),
    NL("klik op een te volgen object; herhaalbaar"),
    RU("щелчок по объекту, который нужно отслеживать; можно повторять"),
    TR("izlenecek bir nesneye tıklama; yinelenebilir"));

SS_MSG(trk_neg_point,
    EN("click on something that is NOT it"),
    JA("それではないものをクリックします"),
    ZH_HANS("点击不属于它的东西"),
    ZH_HANT("點選不屬於它的東西"),
    KO("그것이 아닌 것을 클릭합니다"),
    DE("auf etwas klicken, das es NICHT ist"),
    FR("cliquer sur quelque chose qui n'est PAS lui"),
    ES("clic en algo que NO es"),
    PT("clique em algo que NÃO é"),
    IT("clic su qualcosa che NON è"),
    NL("klik op iets dat het NIET is"),
    RU("щелчок по тому, чем это НЕ является"),
    TR("o OLMAYAN bir şeye tıklama"));

SS_MSG(trk_object,
    EN("end this object, start the next one -- two things need two objects, "
       "since one instance prompted with both fits neither"),
    JA("この対象を終えて次の対象を始めます。2 つのものには 2 つの対象が必要です。"
       "両方でプロンプトされた 1 つのインスタンスはどちらにも合いません"),
    ZH_HANS("结束这个对象、开始下一个 —— 两样东西需要两个对象，"
            "因为用两者一起提示出的单个实例哪个都不合"),
    ZH_HANT("結束這個對象、開始下一個 —— 兩樣東西需要兩個對象，"
            "因為用兩者一起提示出的單個實例哪個都不合"),
    KO("이 대상을 끝내고 다음 대상을 시작합니다 -- 두 가지에는 대상이 둘 필요합니다. "
       "둘 다로 프롬프트한 하나의 인스턴스는 어느 쪽에도 맞지 않기 때문입니다"),
    DE("dieses Objekt beenden, das nächste beginnen -- zwei Dinge brauchen zwei "
       "Objekte, denn eine einzige Instanz, mit beidem geprompt, passt zu keinem"),
    FR("terminer cet objet, commencer le suivant -- deux choses demandent deux "
       "objets, car une seule instance guidée par les deux ne convient à aucune"),
    ES("terminar este objeto y empezar el siguiente: dos cosas necesitan dos "
       "objetos, porque una sola instancia guiada por ambas no encaja con "
       "ninguna"),
    PT("terminar este objeto e começar o seguinte -- duas coisas precisam de "
       "dois objetos, pois uma única instância guiada por ambas não serve a "
       "nenhuma"),
    IT("chiudere questo oggetto e iniziare il successivo -- due cose richiedono "
       "due oggetti, perché una sola istanza guidata da entrambe non va bene per "
       "nessuna"),
    NL("dit object afsluiten, het volgende beginnen -- twee dingen vragen twee "
       "objecten, want één exemplaar dat met beide geprompt is past bij geen van "
       "beide"),
    RU("закрыть этот объект и начать следующий -- на две вещи нужны два объекта, "
       "ведь один экземпляр, заданный обеими подсказками, не подходит ни к одной"),
    TR("bu nesneyi bitir, sonrakine başla -- iki şey iki nesne ister, çünkü "
       "ikisiyle birden istenen tek bir örnek hiçbirine uymaz"));

SS_MSG(trk_at_frame,
    EN("put the clicks that follow on frame n instead of the first, and use them "
       "to correct the object there. Frames are numbered as this command reads "
       "them."),
    JA("以降のクリックを最初のフレームではなくフレーム n に置き、そこで対象を"
       "修正するのに使います。フレーム番号はこのコマンドが読む順です。"),
    ZH_HANS("把随后的点击放到第 n 帧而不是第一帧，并用它们在那里修正对象。"
            "帧号按本命令读取的顺序编号。"),
    ZH_HANT("把隨後的點選放到第 n 格而不是第一格，並用它們在那裡修正對象。"
            "影格編號按本命令讀取的順序編號。"),
    KO("이후의 클릭을 첫 프레임이 아니라 n 번째 프레임에 두고, 거기서 대상을 "
       "고치는 데 씁니다. 프레임 번호는 이 명령이 읽는 순서를 따릅니다."),
    DE("die folgenden Klicks auf Bild n legen statt auf das erste und damit das "
       "Objekt dort berichtigen. Die Bilder sind so nummeriert, wie dieser "
       "Befehl sie liest."),
    FR("placer les clics qui suivent sur l'image n plutôt que sur la première, "
       "et s'en servir pour y corriger l'objet. Les images sont numérotées comme "
       "cette commande les lit."),
    ES("poner los clics siguientes en el fotograma n en vez de en el primero, y "
       "usarlos para corregir allí el objeto. Los fotogramas se numeran como los "
       "lee esta orden."),
    PT("pôr os cliques seguintes no quadro n em vez do primeiro, e usá-los para "
       "corrigir aí o objeto. Os quadros são numerados como este comando os lê."),
    IT("mettere i clic che seguono sul fotogramma n invece che sul primo, e "
       "usarli per correggervi l'oggetto. I fotogrammi sono numerati come questo "
       "comando li legge."),
    NL("de volgende klikken op beeld n zetten in plaats van op het eerste, en ze "
       "gebruiken om het object daar te verbeteren. De beelden zijn genummerd "
       "zoals deze opdracht ze leest."),
    RU("поместить последующие щелчки на кадр n, а не на первый, и поправить ими "
       "объект там. Кадры нумеруются в том порядке, в каком их читает эта "
       "команда."),
    TR("sonraki tıklamaları ilk kare yerine n'inci kareye koy ve orada nesneyi "
       "düzeltmek için kullan. Kareler, bu komutun okuduğu sıraya göre "
       "numaralanır."));

SS_MSG(trk_at_frame_example,
    EN("... is one object clicked once and corrected at frame 90, and a second "
       "object."),
    JA("... は、1 度クリックしてフレーム 90 で修正した対象が 1 つと、"
       "2 つ目の対象、という意味です。"),
    ZH_HANS("……表示一个点击过一次并在第 90 帧修正过的对象，外加第二个对象。"),
    ZH_HANT("……表示一個點選過一次並在第 90 格修正過的對象，外加第二個對象。"),
    KO("... 은 한 번 클릭하고 90번 프레임에서 고친 대상 하나와, 두 번째 대상을 "
       "뜻합니다."),
    DE("... ist ein einmal angeklicktes und bei Bild 90 berichtigtes Objekt, und "
       "ein zweites Objekt."),
    FR("... c'est un objet cliqué une fois et corrigé à l'image 90, plus un "
       "second objet."),
    ES("... es un objeto señalado una vez y corregido en el fotograma 90, más un "
       "segundo objeto."),
    PT("... é um objeto clicado uma vez e corrigido no quadro 90, mais um "
       "segundo objeto."),
    IT("... è un oggetto cliccato una volta e corretto al fotogramma 90, più un "
       "secondo oggetto."),
    NL("... is één object dat eenmaal is aangeklikt en bij beeld 90 verbeterd, "
       "plus een tweede object."),
    RU("... -- это один объект, по которому щёлкнули раз и поправили на кадре 90, "
       "и второй объект."),
    TR("... bir kez tıklanıp 90. karede düzeltilmiş bir nesne ile ikinci bir "
       "nesne demektir."));

SS_MSG(trk_detect_every,
    EN("run the detector every n frames; the memory bank carries tracks in "
       "between"),
    JA("n フレームごとに検出器を走らせます。その間はメモリバンクが追跡を保ちます"),
    ZH_HANS("每 n 帧运行一次检测器；其间由记忆库维持轨迹"),
    ZH_HANT("每 n 格執行一次偵測器；其間由記憶庫維持軌跡"),
    KO("n 프레임마다 검출기를 돌립니다. 그 사이는 메모리 뱅크가 궤적을 이어 갑니다"),
    DE("den Detektor alle n Bilder laufen lassen; dazwischen trägt die "
       "Gedächtnisbank die Spuren"),
    FR("lancer le détecteur toutes les n images ; entre-temps la banque de "
       "mémoire porte les pistes"),
    ES("ejecutar el detector cada n fotogramas; entre medias el banco de memoria "
       "lleva las trazas"),
    PT("executar o detector a cada n quadros; entretanto o banco de memória "
       "carrega as trilhas"),
    IT("eseguire il rivelatore ogni n fotogrammi; nel frattempo la banca di "
       "memoria porta le tracce"),
    NL("de detector elke n beelden draaien; daartussen draagt de geheugenbank de "
       "sporen"),
    RU("запускать детектор каждые n кадров; в промежутках траектории несёт банк "
       "памяти"),
    TR("bulucuyu her n karede bir çalıştır; arada izleri bellek bankası taşır"));

SS_MSG(trk_memory_frames,
    EN("cap spatial memory frames per instance"),
    JA("インスタンスごとの空間メモリのフレーム数に上限を設けます"),
    ZH_HANS("限制每个实例的空间记忆帧数"),
    ZH_HANT("限制每個實例的空間記憶影格數"),
    KO("인스턴스마다 공간 메모리 프레임 수에 상한을 둡니다"),
    DE("räumliche Gedächtnisbilder je Instanz begrenzen"),
    FR("plafonner les images de mémoire spatiale par instance"),
    ES("limitar los fotogramas de memoria espacial por instancia"),
    PT("limitar os quadros de memória espacial por instância"),
    IT("limitare i fotogrammi di memoria spaziale per istanza"),
    NL("de ruimtelijke geheugenbeelden per exemplaar begrenzen"),
    RU("ограничить число кадров пространственной памяти на экземпляр"),
    TR("örnek başına uzamsal bellek karesini sınırla"));

SS_MSG(trk_max_frames,
    EN("stop after n frames"),
    JA("n フレームで停止します"),
    ZH_HANS("处理 n 帧后停止"),
    ZH_HANT("處理 n 格後停止"),
    KO("n 프레임 뒤에 멈춥니다"),
    DE("nach n Bildern anhalten"),
    FR("s'arrêter après n images"),
    ES("parar tras n fotogramas"),
    PT("parar após n quadros"),
    IT("fermarsi dopo n fotogrammi"),
    NL("na n beelden stoppen"),
    RU("остановиться после n кадров"),
    TR("n kareden sonra dur"));

SS_MSG(trk_out,
    EN("write a per-frame binary mask PNG"),
    JA("フレームごとの 2 値マスク PNG を書き出します"),
    ZH_HANS("逐帧写出二值掩码 PNG"),
    ZH_HANT("逐格寫出二值遮罩 PNG"),
    KO("프레임마다 이진 마스크 PNG 를 씁니다"),
    DE("je Bild ein binäres Masken-PNG schreiben"),
    FR("écrire un PNG de masque binaire par image"),
    ES("escribir un PNG de máscara binaria por fotograma"),
    PT("escrever um PNG de máscara binária por quadro"),
    IT("scrivere un PNG di maschera binaria per fotogramma"),
    NL("per beeld een binair masker-PNG schrijven"),
    RU("записывать двоичный PNG маски для каждого кадра"),
    TR("kare başına ikili maske PNG'si yaz"));

SS_MSG(trk_keep_prompted,
    EN("white = the prompted objects. By default they are BLACK and everything "
       "else is white, which is what a reconstruction pipeline wants from \"mask "
       "out the people\""),
    JA("白 = プロンプトで指した対象。既定では対象が黒で、それ以外が白になります。"
       "「人を消す」と言われた再構成のパイプラインが欲しいのはこちらです"),
    ZH_HANS("白色 = 提示指定的对象。默认相反：对象为黑、其余为白，"
            "这正是重建流程听到“把人遮掉”时想要的"),
    ZH_HANT("白色 = 提示指定的對象。預設相反：對象為黑、其餘為白，"
            "這正是重建流程聽到「把人遮掉」時想要的"),
    KO("흰색 = 프롬프트가 가리킨 대상. 기본값은 그 반대로 대상이 검고 나머지가 "
       "흰데, \"사람을 지워\"라는 말을 들은 재구성 파이프라인이 원하는 것이 그쪽입니다"),
    DE("weiß = die geprompteten Objekte. Standardmäßig sind sie SCHWARZ und "
       "alles andere weiß, und genau das will eine Rekonstruktionspipeline von "
       "\"die Leute wegmaskieren\""),
    FR("blanc = les objets désignés. Par défaut ils sont NOIRS et tout le reste "
       "blanc, ce qu'attend une chaîne de reconstruction quand on lui dit "
       "« masque les gens »"),
    ES("blanco = los objetos indicados. Por defecto son NEGROS y todo lo demás "
       "blanco, que es lo que una cadena de reconstrucción quiere de «tapa a la "
       "gente»"),
    PT("branco = os objetos indicados. Por padrão são PRETOS e tudo o resto "
       "branco, que é o que uma cadeia de reconstrução quer de \"mascare as "
       "pessoas\""),
    IT("bianco = gli oggetti indicati. Per impostazione predefinita sono NERI e "
       "tutto il resto è bianco, che è ciò che una catena di ricostruzione vuole "
       "da \"maschera le persone\""),
    NL("wit = de geprompte objecten. Standaard zijn ze ZWART en al het andere "
       "wit, en dat is wat een reconstructieketen wil van \"maskeer de mensen "
       "weg\""),
    RU("белое = объекты из подсказки. По умолчанию они ЧЁРНЫЕ, а всё остальное "
       "белое -- именно этого ждёт конвейер реконструкции от «убери людей»"),
    TR("beyaz = istemle belirtilen nesneler. Varsayılan olarak onlar SİYAH, geri "
       "kalan her şey beyazdır; \"insanları maskele\" denince bir yeniden "
       "oluşturma hattının istediği budur"));

SS_MSG(mask_dilate,
    EN("grow every detection by this share of its own size before it joins the "
       "mask, covering the rim of colour a tight outline leaves behind. 0 turns "
       "the margin off"),
    JA("マスクに加える前に、検出ごとに自身の大きさのこの割合だけ膨らませます。"
       "輪郭がきつすぎて残る色の縁を覆うためです。0 で余白なし"),
    ZH_HANS("在并入蒙版之前，把每个检测按自身尺寸的这个比例向外扩张，"
            "用来盖住紧贴轮廓留下的一圈残色。填 0 关闭"),
    ZH_HANT("在併入遮罩之前，把每個偵測按自身尺寸的這個比例向外擴張，"
            "用來蓋住緊貼輪廓留下的一圈殘色。填 0 關閉"),
    KO("마스크에 합치기 전에 각 검출을 자기 크기의 이 비율만큼 넓힙니다. 윤곽선이 "
       "너무 딱 맞아 남는 테두리 색을 덮기 위해서입니다. 0이면 여백 없음"),
    DE("jede Erkennung vor dem Zusammenfassen um diesen Anteil ihrer eigenen "
       "Größe wachsen lassen, was den Farbsaum abdeckt, den eine zu enge Kontur "
       "zurücklässt. 0 schaltet den Rand ab"),
    FR("élargir chaque détection de cette fraction de sa propre taille avant de "
       "la fondre dans le masque, ce qui couvre le liseré que laisse un contour "
       "trop serré. 0 supprime la marge"),
    ES("agrandar cada detección en esta fracción de su propio tamaño antes de "
       "unirla a la máscara, lo que tapa el borde que deja un contorno "
       "demasiado ceñido. 0 quita el margen"),
    PT("aumentar cada detecção nesta fração do seu próprio tamanho antes de a "
       "juntar à máscara, o que tapa a orla que um contorno demasiado justo "
       "deixa. 0 desliga a margem"),
    IT("ingrandire ogni rilevamento di questa frazione della propria dimensione "
       "prima di unirlo alla maschera, il che copre il bordo che un contorno "
       "troppo aderente lascia. 0 toglie il margine"),
    NL("elke detectie met dit deel van haar eigen grootte laten groeien voordat "
       "ze in het masker opgaat, wat de kleurrand dekt die een te strakke "
       "omtrek achterlaat. 0 zet de marge uit"),
    RU("увеличить каждое обнаружение на эту долю его собственного размера перед "
       "объединением в маску: это закрывает кайму, которую оставляет слишком "
       "плотный контур. 0 отключает поле"),
    TR("her algılamayı maskeye katılmadan önce kendi boyutunun bu oranı kadar "
       "büyütür; böylece fazla dar bir sınırın bıraktığı renk kenarı kapanır. "
       "0 payı kapatır"));

SS_MSG(trk_overlay,
    EN("write a colour overlay instead"),
    JA("代わりにカラーのオーバーレイを書き出します"),
    ZH_HANS("改为写出彩色叠加图"),
    ZH_HANT("改為寫出彩色疊加圖"),
    KO("대신 컬러 오버레이를 씁니다"),
    DE("stattdessen eine farbige Überlagerung schreiben"),
    FR("écrire plutôt une superposition en couleur"),
    ES("escribir en su lugar una superposición en color"),
    PT("escrever antes uma sobreposição a cores"),
    IT("scrivere invece una sovrapposizione a colori"),
    NL("in plaats daarvan een kleurenoverlay schrijven"),
    RU("вместо этого записать цветное наложение"),
    TR("bunun yerine renkli bir bindirme yaz"));

// ---- video, extract, and the common tail ----

SS_MSG(cmd_video,
    EN("Probe a video file and report codec, geometry and decode support."),
    JA("動画ファイルを調べ、コーデック・寸法・デコード対応を報告します。"),
    ZH_HANS("探查视频文件，报告编解码器、几何尺寸与解码支持情况。"),
    ZH_HANT("探查影片檔案，回報編解碼器、幾何尺寸與解碼支援情況。"),
    KO("동영상 파일을 살펴 코덱, 크기, 디코딩 지원 여부를 알려 줍니다."),
    DE("Eine Videodatei prüfen und Codec, Geometrie und Dekodierunterstützung "
       "melden."),
    FR("Sonder un fichier vidéo et indiquer codec, géométrie et prise en charge "
       "du décodage."),
    ES("Sondear un archivo de vídeo e informar de códec, geometría y soporte de "
       "descodificación."),
    PT("Sondar um arquivo de vídeo e relatar códec, geometria e suporte de "
       "decodificação."),
    IT("Sondare un file video e riferire codec, geometria e supporto alla "
       "decodifica."),
    NL("Een videobestand aftasten en codec, afmetingen en decodeerondersteuning "
       "melden."),
    RU("Изучить видеофайл и сообщить кодек, геометрию и поддержку декодирования."),
    TR("Bir video dosyasını yokla; kodek, geometri ve çözme desteğini bildir."));

SS_MSG(cmd_extract,
    EN("Write the sharpest frames of a video, optionally masked."),
    JA("動画の中で最も鮮鋭なフレームを書き出します。必要ならマスクも付けます。"),
    ZH_HANS("写出视频中最清晰的帧，可选地附带遮罩。"),
    ZH_HANT("寫出影片中最清晰的影格，可選地附帶遮罩。"),
    KO("동영상에서 가장 선명한 프레임을 씁니다. 원하면 마스크도 함께."),
    DE("Die schärfsten Einzelbilder eines Videos schreiben, wahlweise maskiert."),
    FR("Écrire les images les plus nettes d'une vidéo, éventuellement masquées."),
    ES("Escribir los fotogramas más nítidos de un vídeo, con máscara si se pide."),
    PT("Escrever os quadros mais nítidos de um vídeo, com máscara se pedido."),
    IT("Scrivere i fotogrammi più nitidi di un video, con maschera se richiesto."),
    NL("De scherpste beelden van een video schrijven, desgewenst gemaskeerd."),
    RU("Записать самые резкие кадры видео, при желании с маской."),
    TR("Bir videonun en keskin karelerini yaz, istenirse maskeli."));

SS_MSG(cmd_extract_more,
    EN("`{0} extract --help` lists its own options."),
    JA("独自のオプションは `{0} extract --help` に一覧があります。"),
    ZH_HANS("它自己的选项见 `{0} extract --help`。"),
    ZH_HANT("它自己的選項見 `{0} extract --help`。"),
    KO("자체 옵션은 `{0} extract --help` 에 있습니다."),
    DE("`{0} extract --help` listet die eigenen Optionen auf."),
    FR("`{0} extract --help` liste ses propres options."),
    ES("`{0} extract --help` enumera sus propias opciones."),
    PT("`{0} extract --help` lista as suas próprias opções."),
    IT("`{0} extract --help` elenca le sue opzioni."),
    NL("`{0} extract --help` somt zijn eigen opties op."),
    RU("`{0} extract --help` перечисляет его собственные параметры."),
    TR("`{0} extract --help` kendi seçeneklerini listeler."));

SS_MSG(label_common,
    EN("Common:"), JA("共通:"), ZH_HANS("通用："), ZH_HANT("通用："),
    KO("공통:"), DE("Gemeinsam:"), FR("Communs :"), ES("Comunes:"),
    PT("Comuns:"), IT("Comuni:"), NL("Gemeenschappelijk:"), RU("Общие:"),
    TR("Ortak:"));

SS_MSG(label_environment,
    EN("Environment:"), JA("環境変数:"), ZH_HANS("环境变量："),
    ZH_HANT("環境變數："), KO("환경 변수:"), DE("Umgebung:"),
    FR("Environnement :"), ES("Entorno:"), PT("Ambiente:"), IT("Ambiente:"),
    NL("Omgeving:"), RU("Переменные окружения:"), TR("Ortam:"));

SS_MSG(common_image_gamut,
    EN("colour primaries the frames are in; they convert to sRGB first"),
    JA("フレームの色域。先に sRGB へ変換されます"),
    ZH_HANS("帧所处的色域；会先转换为 sRGB"),
    ZH_HANT("影格所處的色域；會先轉換為 sRGB"),
    KO("프레임의 색역. 먼저 sRGB 로 변환됩니다"),
    DE("Farbprimärvalenzen der Bilder; sie werden zuerst nach sRGB gewandelt"),
    FR("primaires de couleur des images ; elles passent d'abord en sRGB"),
    ES("primarios de color de los fotogramas; primero pasan a sRGB"),
    PT("primárias de cor dos quadros; passam primeiro a sRGB"),
    IT("primarie di colore dei fotogrammi; passano prima a sRGB"),
    NL("kleurprimairen van de frames; ze gaan eerst naar sRGB"),
    RU("основные цвета кадров; сначала они переводятся в sRGB"),
    TR("karelerin renk birincilleri; önce sRGB'ye çevrilirler"));

SS_MSG(common_image_linear,
    EN("frames are linear light, not display-encoded"),
    JA("フレームは表示用エンコードではなくリニア光です"),
    ZH_HANS("帧是线性光而非显示编码"),
    ZH_HANT("影格是線性光而非顯示編碼"),
    KO("프레임은 디스플레이 인코딩이 아니라 선형 광입니다"),
    DE("Bilder sind lineares Licht, nicht anzeigecodiert"),
    FR("les images sont en lumière linéaire, non encodées pour l'affichage"),
    ES("los fotogramas son luz lineal, no codificados para pantalla"),
    PT("os quadros são luz linear, não codificados para exibição"),
    IT("i fotogrammi sono luce lineare, non codificati per lo schermo"),
    NL("frames zijn lineair licht, niet weergavegecodeerd"),
    RU("кадры -- линейный свет, а не экранное кодирование"),
    TR("kareler ekran kodlu değil, doğrusal ışıktır"));

SS_MSG(common_max_size,
    EN("downscale inputs to fit (default 1600, 0 = off)"),
    JA("入力をこの大きさに収まるよう縮小します（既定 1600、0 で無効）"),
    ZH_HANS("把输入缩小到不超过该尺寸（默认 1600，0 表示关闭）"),
    ZH_HANT("把輸入縮小到不超過該尺寸（預設 1600，0 表示關閉）"),
    KO("입력을 이 크기에 맞도록 줄입니다(기본 1600, 0 이면 끔)"),
    DE("Eingaben passend verkleinern (Vorgabe 1600, 0 = aus)"),
    FR("réduire les entrées pour tenir dans cette taille (défaut 1600, 0 = "
       "désactivé)"),
    ES("reducir las entradas para que quepan (por defecto 1600, 0 = desactivado)"),
    PT("reduzir as entradas para caberem (padrão 1600, 0 = desligado)"),
    IT("ridurre gli ingressi perché ci stiano (predefinito 1600, 0 = disattivo)"),
    NL("invoer verkleinen zodat die past (standaard 1600, 0 = uit)"),
    RU("уменьшать входные изображения до этого размера (по умолчанию 1600, "
       "0 -- выключено)"),
    TR("girdileri sığacak şekilde küçült (varsayılan 1600, 0 = kapalı)"));

// ===========================================================================
// `spirula sam extract --help`
// ===========================================================================

SS_MSG(xh_frame_selection,
    EN("Frame selection"),
    JA("フレームの選択"),
    ZH_HANS("选帧"),
    ZH_HANT("選格"),
    KO("프레임 고르기"),
    DE("Bildauswahl"),
    FR("Choix des images"),
    ES("Elección de fotogramas"),
    PT("Escolha de quadros"),
    IT("Scelta dei fotogrammi"),
    NL("Beeldkeuze"),
    RU("Выбор кадров"),
    TR("Kare seçimi"));

SS_MSG(xh_360_section,
    EN("360 cameras:"),
    JA("360 度カメラ:"),
    ZH_HANS("360 相机："),
    ZH_HANT("360 相機："),
    KO("360 카메라:"),
    DE("360-Kameras:"),
    FR("Caméras 360 :"),
    ES("Cámaras 360:"),
    PT("Câmeras 360:"),
    IT("Fotocamere 360:"),
    NL("360-camera's:"),
    RU("Камеры 360:"),
    TR("360 kameralar:"));

SS_MSG(xh_360,
    EN("what a GoPro MAX .360 becomes: faces (ten seam-free perspective views, "
       "five per lens), equirect (one 2:1 panorama) or off (the raw tracks). "
       "Default: faces"),
    JA("GoPro MAX の .360 の変換先: faces（継ぎ目のない透視ビュー 10 枚、レンズ"
       "ごとに 5 枚）、equirect（2:1 のパノラマ 1 枚）、off（生のトラック）。既定は "
       "faces"),
    ZH_HANS("GoPro MAX .360 的展开方式：faces（十个无拼缝透视视角，每镜头五个）、"
            "equirect（一张 2:1 全景）或 off（原始轨道）。默认 faces"),
    ZH_HANT("GoPro MAX .360 的展開方式：faces（十個無拼縫透視視角，每鏡頭五個）、"
            "equirect（一張 2:1 全景）或 off（原始軌道）。預設 faces"),
    KO("GoPro MAX .360을 무엇으로 바꿀지: faces(이음매 없는 원근 뷰 10장, 렌즈당 "
       "5장), equirect(2:1 파노라마 1장), off(원본 트랙). 기본값 faces"),
    DE("wozu eine GoPro-MAX-.360 wird: faces (zehn nahtfreie perspektivische "
       "Ansichten, fünf je Objektiv), equirect (ein 2:1-Panorama) oder off (die "
       "rohen Spuren). Vorgabe: faces"),
    FR("ce que devient un .360 GoPro MAX : faces (dix vues en perspective sans "
       "couture, cinq par objectif), equirect (un panorama 2:1) ou off (les "
       "pistes brutes). Défaut : faces"),
    ES("en qué se convierte un .360 de GoPro MAX: faces (diez vistas en "
       "perspectiva sin costuras, cinco por objetivo), equirect (un panorama "
       "2:1) u off (las pistas en bruto). Por defecto: faces"),
    PT("no que um .360 da GoPro MAX se torna: faces (dez vistas em perspectiva "
       "sem emendas, cinco por lente), equirect (um panorama 2:1) ou off (as "
       "faixas brutas). Padrão: faces"),
    IT("in che cosa diventa un .360 GoPro MAX: faces (dieci viste prospettiche "
       "senza cuciture, cinque per obiettivo), equirect (un panorama 2:1) "
       "oppure off (le tracce grezze). Predefinito: faces"),
    NL("wat een GoPro MAX-.360 wordt: faces (tien naadloze perspectiefaanzichten, "
       "vijf per lens), equirect (één 2:1-panorama) of off (de ruwe sporen). "
       "Standaard: faces"),
    RU("во что превращается .360 с GoPro MAX: faces (десять перспективных видов "
       "без швов, по пять на объектив), equirect (одна панорама 2:1) или off "
       "(исходные дорожки). По умолчанию: faces"),
    TR("GoPro MAX .360 dosyasının neye dönüşeceği: faces (dikişsiz on perspektif "
       "görünüm, objektif başına beş), equirect (tek 2:1 panorama) veya off (ham "
       "izler). Varsayılan: faces"));

SS_MSG(xh_360_size,
    EN("pixels a side for each face, or the width of the equirectangular "
       "image; default is from the source"),
    JA("各ビューの一辺のピクセル数、または正距円筒画像の幅。既定は入力から決めます"),
    ZH_HANS("每个视角的边长像素，或等距柱状图像的宽度；默认由输入决定"),
    ZH_HANT("每個視角的邊長像素，或等距柱狀影像的寬度；預設由輸入決定"),
    KO("각 뷰의 한 변 픽셀 수 또는 등장방형 이미지의 너비. 기본값은 입력에서 정합니다"),
    DE("Pixel je Kante einer Ansicht oder die Breite des äquirektangulären "
       "Bildes; Vorgabe richtet sich nach der Quelle"),
    FR("pixels de côté pour chaque vue, ou largeur de l'image équirectangulaire ; "
       "par défaut d'après la source"),
    ES("píxeles de lado de cada vista, o el ancho de la imagen equirectangular; "
       "por defecto según la fuente"),
    PT("pixels de lado de cada vista, ou a largura da imagem equirretangular; "
       "por padrão conforme a fonte"),
    IT("pixel di lato per ogni vista, o la larghezza dell'immagine "
       "equirettangolare; per impostazione predefinita dalla sorgente"),
    NL("pixels per zijde van elk aanzicht, of de breedte van het "
       "equirectangulaire beeld; standaard uit de bron"),
    RU("пикселей на сторону каждого вида или ширина равнопромежуточного "
       "изображения; по умолчанию — из источника"),
    TR("her görünümün kenar piksel sayısı ya da eş dikdörtgen görüntünün "
       "genişliği; varsayılan kaynaktan alınır"));

SS_MSG(xh_360_orient,
    EN("how the camera was held: yaw,pitch,roll in degrees, applied to the "
       "whole sphere. 0,0,180 turns an inverted mount the right way up"),
    JA("カメラの向き: ヨー,ピッチ,ロール（度）。全天球に適用します。逆さ付けは "
       "0,0,180 で正立します"),
    ZH_HANS("相机的姿态：偏航,俯仰,翻滚（度），作用于整个球面。倒装时用 0,0,180 摆正"),
    ZH_HANT("相機的姿態：偏航,俯仰,翻滾（度），作用於整個球面。倒裝時用 0,0,180 擺正"),
    KO("카메라를 든 방향: 요,피치,롤(도)이며 구 전체에 적용됩니다. 거꾸로 장착했다면 "
       "0,0,180"),
    DE("wie die Kamera gehalten wurde: Gier,Nick,Roll in Grad, auf die ganze "
       "Kugel angewandt. 0,0,180 stellt eine kopfüber montierte Kamera aufrecht"),
    FR("comment la caméra était tenue : lacet,tangage,roulis en degrés, "
       "appliqués à toute la sphère. 0,0,180 redresse un montage à l'envers"),
    ES("cómo se sostuvo la cámara: guiñada,cabeceo,alabeo en grados, aplicados "
       "a toda la esfera. 0,0,180 endereza un montaje invertido"),
    PT("como a câmera foi segurada: guinada,arfagem,rolagem em graus, "
       "aplicados a toda a esfera. 0,0,180 endireita uma montagem invertida"),
    IT("come era tenuta la fotocamera: imbardata,beccheggio,rollio in gradi, "
       "applicati a tutta la sfera. 0,0,180 raddrizza un montaggio capovolto"),
    NL("hoe de camera gehouden werd: gier,stamp,rol in graden, op de hele bol "
       "toegepast. 0,0,180 zet een omgekeerde montage rechtop"),
    RU("как держали камеру: рыскание,тангаж,крен в градусах для всей сферы. "
       "0,0,180 переворачивает перевёрнутое крепление"),
    TR("kameranın nasıl tutulduğu: derece cinsinden sapma,yunuslama,yalpa; "
       "tüm küreye uygulanır. 0,0,180 ters takılan kamerayı düzeltir"));

SS_MSG(xh_masking,
    EN("Masking (needs --model)"),
    JA("マスク（--model が必要）"),
    ZH_HANS("遮罩（需要 --model）"),
    ZH_HANT("遮罩（需要 --model）"),
    KO("마스킹(--model 필요)"),
    DE("Maskierung (braucht --model)"),
    FR("Masquage (demande --model)"),
    ES("Enmascarado (necesita --model)"),
    PT("Máscara (precisa de --model)"),
    IT("Mascheratura (richiede --model)"),
    NL("Maskeren (heeft --model nodig)"),
    RU("Маскирование (нужен --model)"),
    TR("Maskeleme (--model ister)"));

SS_MSG(xh_out,
    EN("output directory (default: <video-without-ext>/images)"),
    JA("出力ディレクトリ（既定: <拡張子を除いた動画名>/images）"),
    ZH_HANS("输出目录（默认：<去掉扩展名的视频名>/images）"),
    ZH_HANT("輸出目錄（預設：<去掉副檔名的影片名>/images）"),
    KO("출력 디렉터리(기본값: <확장자를 뺀 동영상 이름>/images)"),
    DE("Ausgabeverzeichnis (Vorgabe: <Video ohne Endung>/images)"),
    FR("dossier de sortie (défaut : <vidéo sans extension>/images)"),
    ES("carpeta de salida (por defecto: <vídeo sin extensión>/images)"),
    PT("pasta de saída (padrão: <vídeo sem extensão>/images)"),
    IT("cartella di uscita (predefinito: <video senza estensione>/images)"),
    NL("uitvoermap (standaard: <video zonder extensie>/images)"),
    RU("каталог вывода (по умолчанию: <видео без расширения>/images)"),
    TR("çıktı dizini (varsayılan: <uzantısız video>/images)"));

SS_MSG(xh_skip,
    EN("write one frame every n source frames (default 1)"),
    JA("元の n フレームごとに 1 枚を書き出します（既定 1）"),
    ZH_HANS("每 n 个源帧写出一帧（默认 1）"),
    ZH_HANT("每 n 個來源影格寫出一格（預設 1）"),
    KO("원본 n 프레임마다 한 장을 씁니다(기본 1)"),
    DE("je n Quellbilder ein Bild schreiben (Vorgabe 1)"),
    FR("écrire une image toutes les n images source (défaut 1)"),
    ES("escribir un fotograma cada n fotogramas de origen (por defecto 1)"),
    PT("escrever um quadro a cada n quadros de origem (padrão 1)"),
    IT("scrivere un fotogramma ogni n fotogrammi sorgente (predefinito 1)"),
    NL("elke n bronbeelden één beeld schrijven (standaard 1)"),
    RU("записывать один кадр из каждых n исходных (по умолчанию 1)"),
    TR("her n kaynak karede bir kare yaz (varsayılan 1)"));

SS_MSG(xh_keep,
    EN("choose the sharpest of the last n frames; -1 = round(skip/2) (default), "
       "0 = no selection"),
    JA("直近 n フレームのうち最も鮮鋭なものを選びます。-1 で round(skip/2)（既定）、"
       "0 で選択なし"),
    ZH_HANS("在最近 n 帧中选最清晰的一帧；-1 表示 round(skip/2)（默认），0 表示不做选择"),
    ZH_HANT("在最近 n 格中選最清晰的一格；-1 表示 round(skip/2)（預設），0 表示不做選擇"),
    KO("최근 n 프레임 중 가장 선명한 것을 고릅니다. -1 이면 round(skip/2)(기본), "
       "0 이면 고르지 않음"),
    DE("das schärfste der letzten n Bilder wählen; -1 = round(skip/2) (Vorgabe), "
       "0 = keine Auswahl"),
    FR("choisir la plus nette des n dernières images ; -1 = round(skip/2) "
       "(défaut), 0 = pas de choix"),
    ES("elegir el más nítido de los últimos n fotogramas; -1 = round(skip/2) "
       "(por defecto), 0 = sin elección"),
    PT("escolher o mais nítido dos últimos n quadros; -1 = round(skip/2) "
       "(padrão), 0 = sem escolha"),
    IT("scegliere il più nitido degli ultimi n fotogrammi; -1 = round(skip/2) "
       "(predefinito), 0 = nessuna scelta"),
    NL("het scherpste van de laatste n beelden kiezen; -1 = round(skip/2) "
       "(standaard), 0 = geen keuze"),
    RU("выбирать самый резкий из последних n кадров; -1 = round(skip/2) "
       "(по умолчанию), 0 -- без выбора"),
    TR("son n karenin en keskinini seç; -1 = round(skip/2) (varsayılan), 0 = "
       "seçim yok"));

SS_MSG(xh_max_frames,
    EN("stop after writing n frames"),
    JA("n 枚書き出したら停止します"),
    ZH_HANS("写出 n 帧后停止"),
    ZH_HANT("寫出 n 格後停止"),
    KO("n 장을 쓰고 나면 멈춥니다"),
    DE("nach n geschriebenen Bildern anhalten"),
    FR("s'arrêter après avoir écrit n images"),
    ES("parar tras escribir n fotogramas"),
    PT("parar depois de escrever n quadros"),
    IT("fermarsi dopo aver scritto n fotogrammi"),
    NL("stoppen na het schrijven van n beelden"),
    RU("остановиться, записав n кадров"),
    TR("n kare yazdıktan sonra dur"));

SS_MSG(xh_quality,
    EN("JPEG quality; outside that range writes PNG (default 95)"),
    JA("JPEG の品質。範囲外の値では PNG を書き出します（既定 95）"),
    ZH_HANS("JPEG 质量；超出该范围则写出 PNG（默认 95）"),
    ZH_HANT("JPEG 品質；超出該範圍則寫出 PNG（預設 95）"),
    KO("JPEG 품질. 범위를 벗어나면 PNG 로 씁니다(기본 95)"),
    DE("JPEG-Qualität; außerhalb dieses Bereichs wird PNG geschrieben "
       "(Vorgabe 95)"),
    FR("qualité JPEG ; hors de cette plage, écrit du PNG (défaut 95)"),
    ES("calidad JPEG; fuera de ese rango escribe PNG (por defecto 95)"),
    PT("qualidade JPEG; fora dessa faixa escreve PNG (padrão 95)"),
    IT("qualità JPEG; fuori da quell'intervallo scrive PNG (predefinito 95)"),
    NL("JPEG-kwaliteit; buiten dat bereik wordt PNG geschreven (standaard 95)"),
    RU("качество JPEG; вне этого диапазона пишется PNG (по умолчанию 95)"),
    TR("JPEG kalitesi; bu aralığın dışında PNG yazar (varsayılan 95)"));

SS_MSG(xh_rotate,
    EN("0, 90, 180 or 270, clockwise"),
    JA("0、90、180、270 度（時計回り）"),
    ZH_HANS("0、90、180 或 270 度，顺时针"),
    ZH_HANT("0、90、180 或 270 度，順時針"),
    KO("0, 90, 180, 270도(시계 방향)"),
    DE("0, 90, 180 oder 270, im Uhrzeigersinn"),
    FR("0, 90, 180 ou 270, sens horaire"),
    ES("0, 90, 180 o 270, en sentido horario"),
    PT("0, 90, 180 ou 270, no sentido horário"),
    IT("0, 90, 180 o 270, in senso orario"),
    NL("0, 90, 180 of 270, met de klok mee"),
    RU("0, 90, 180 или 270 по часовой стрелке"),
    TR("0, 90, 180 ya da 270, saat yönünde"));

SS_MSG(xh_scale,
    EN("resize factor, at most 1"),
    JA("リサイズ倍率。1 以下"),
    ZH_HANS("缩放系数，不大于 1"),
    ZH_HANT("縮放係數，不大於 1"),
    KO("크기 조정 배율. 1 이하"),
    DE("Skalierungsfaktor, höchstens 1"),
    FR("facteur de redimensionnement, au plus 1"),
    ES("factor de reescalado, como mucho 1"),
    PT("fator de redimensionamento, no máximo 1"),
    IT("fattore di ridimensionamento, al massimo 1"),
    NL("schaalfactor, hoogstens 1"),
    RU("коэффициент масштабирования, не больше 1"),
    TR("yeniden boyutlandırma katsayısı, en çok 1"));

SS_MSG(xh_track,
    EN("video track to read; default is every track, written to <out>/cam0, "
       "<out>/cam1, ..."),
    JA("読み取る動画トラック。既定はすべてのトラックで、<out>/cam0、<out>/cam1、... に"
       "書き出します"),
    ZH_HANS("要读取的视频轨道；默认读取全部轨道，分别写入 <out>/cam0、<out>/cam1……"),
    ZH_HANT("要讀取的影片軌道；預設讀取全部軌道，分別寫入 <out>/cam0、<out>/cam1……"),
    KO("읽을 비디오 트랙. 기본은 모든 트랙이며 <out>/cam0, <out>/cam1, ... 에 "
       "씁니다"),
    DE("zu lesende Videospur; Vorgabe ist jede Spur, geschrieben nach "
       "<out>/cam0, <out>/cam1, ..."),
    FR("piste vidéo à lire ; par défaut toutes les pistes, écrites dans "
       "<out>/cam0, <out>/cam1, ..."),
    ES("pista de vídeo que leer; por defecto todas, escritas en <out>/cam0, "
       "<out>/cam1, ..."),
    PT("faixa de vídeo a ler; por padrão todas, escritas em <out>/cam0, "
       "<out>/cam1, ..."),
    IT("traccia video da leggere; per impostazione predefinita tutte, scritte in "
       "<out>/cam0, <out>/cam1, ..."),
    NL("videospoor om te lezen; standaard elk spoor, geschreven naar <out>/cam0, "
       "<out>/cam1, ..."),
    RU("какую видеодорожку читать; по умолчанию все, они пишутся в <out>/cam0, "
       "<out>/cam1, ..."),
    TR("okunacak video izi; varsayılan olarak her iz, <out>/cam0, <out>/cam1, "
       "... içine yazılır"));

SS_MSG(xh_threads,
    EN("image-encoder threads (default: cores - 1)"),
    JA("画像エンコーダのスレッド数（既定: コア数 - 1）"),
    ZH_HANS("图像编码线程数（默认：核心数 - 1）"),
    ZH_HANT("影像編碼執行緒數（預設：核心數 - 1）"),
    KO("이미지 인코더 스레드 수(기본값: 코어 수 - 1)"),
    DE("Threads des Bildkodierers (Vorgabe: Kerne - 1)"),
    FR("fils de l'encodeur d'images (défaut : cœurs - 1)"),
    ES("hilos del codificador de imágenes (por defecto: núcleos - 1)"),
    PT("threads do codificador de imagens (padrão: núcleos - 1)"),
    IT("thread del codificatore di immagini (predefinito: core - 1)"),
    NL("threads van de beeldcodeerder (standaard: kernen - 1)"),
    RU("потоки кодировщика изображений (по умолчанию: ядра - 1)"),
    TR("görüntü kodlayıcı iş parçacıkları (varsayılan: çekirdek - 1)"));

SS_MSG(xh_model,
    EN("SAM 3 checkpoint"),
    JA("SAM 3 のチェックポイント"),
    ZH_HANS("SAM 3 检查点"),
    ZH_HANT("SAM 3 檢查點"),
    KO("SAM 3 체크포인트"),
    DE("SAM-3-Prüfpunkt"),
    FR("point de contrôle SAM 3"),
    ES("punto de control de SAM 3"),
    PT("ponto de verificação do SAM 3"),
    IT("checkpoint SAM 3"),
    NL("SAM 3-controlepunt"),
    RU("контрольная точка SAM 3"),
    TR("SAM 3 denetim noktası"));

SS_MSG(xh_text,
    EN("semicolon-separated noun phrases, e.g. \"person; car\""),
    JA("セミコロン区切りの名詞句。例: \"person; car\""),
    ZH_HANS("以分号分隔的名词短语，例如 \"person; car\""),
    ZH_HANT("以分號分隔的名詞片語，例如 \"person; car\""),
    KO("세미콜론으로 나눈 명사구. 예: \"person; car\""),
    DE("durch Semikolon getrennte Nominalphrasen, z. B. \"person; car\""),
    FR("groupes nominaux séparés par des points-virgules, p. ex. \"person; car\""),
    ES("sintagmas nominales separados por punto y coma, p. ej. \"person; car\""),
    PT("sintagmas nominais separados por ponto e vírgula, p. ex. \"person; car\""),
    IT("sintagmi nominali separati da punto e virgola, ad es. \"person; car\""),
    NL("zelfstandignaamwoordgroepen gescheiden door puntkomma's, bv. \"person; "
       "car\""),
    RU("именные группы через точку с запятой, например \"person; car\""),
    TR("noktalı virgülle ayrılmış ad öbekleri, örn. \"person; car\""));

SS_MSG(xh_neg_text,
    EN("phrases to subtract from the mask"),
    JA("マスクから差し引く語句"),
    ZH_HANS("要从掩码中减去的短语"),
    ZH_HANT("要從遮罩中減去的片語"),
    KO("마스크에서 빼낼 어구"),
    DE("Phrasen, die von der Maske abgezogen werden"),
    FR("expressions à soustraire du masque"),
    ES("expresiones que restar de la máscara"),
    PT("expressões a subtrair da máscara"),
    IT("espressioni da sottrarre alla maschera"),
    NL("frasen die van het masker afgetrokken worden"),
    RU("выражения, вычитаемые из маски"),
    TR("maskeden çıkarılacak ifadeler"));

SS_MSG(xh_mask_mode,
    EN("video (default) tracks instances across frames; image treats every "
       "written frame independently"),
    JA("video（既定）はフレームをまたいでインスタンスを追跡し、image は書き出す"
       "各フレームを独立に扱います"),
    ZH_HANS("video（默认）跨帧跟踪实例；image 则把写出的每一帧各自独立处理"),
    ZH_HANT("video（預設）跨格追蹤實例；image 則把寫出的每一格各自獨立處理"),
    KO("video(기본)는 프레임을 넘나들며 인스턴스를 추적하고, image 는 쓰는 프레임을 "
       "각각 따로 다룹니다"),
    DE("video (Vorgabe) verfolgt Instanzen über die Bilder hinweg; image "
       "behandelt jedes geschriebene Bild für sich"),
    FR("video (défaut) suit les instances d'une image à l'autre ; image traite "
       "chaque image écrite séparément"),
    ES("video (por defecto) sigue las instancias de un fotograma a otro; image "
       "trata cada fotograma escrito por separado"),
    PT("video (padrão) rastreia as instâncias de quadro em quadro; image trata "
       "cada quadro escrito à parte"),
    IT("video (predefinito) traccia le istanze tra i fotogrammi; image tratta "
       "ogni fotogramma scritto a sé"),
    NL("video (standaard) volgt exemplaren over de beelden heen; image behandelt "
       "elk geschreven beeld apart"),
    RU("video (по умолчанию) отслеживает экземпляры между кадрами; image "
       "обрабатывает каждый записанный кадр отдельно"),
    TR("video (varsayılan) örnekleri kareler boyunca izler; image yazılan her "
       "kareyi ayrı ele alır"));

SS_MSG(xh_mask_keep,
    EN("background (default): the prompt names distractors and everything else "
       "is kept; subject: the prompt names what to keep and everything else is "
       "masked out"),
    JA("background（既定）: プロンプトは邪魔なものを指し、それ以外を残します。"
       "subject: プロンプトは残すものを指し、それ以外をマスクで消します"),
    ZH_HANS("background（默认）：提示词指的是干扰物，其余一律保留；"
            "subject：提示词指的是要保留的东西，其余一律遮掉"),
    ZH_HANT("background（預設）：提示詞指的是干擾物，其餘一律保留；"
            "subject：提示詞指的是要保留的東西，其餘一律遮掉"),
    KO("background(기본): 프롬프트가 방해물을 가리키고 나머지를 남깁니다. "
       "subject: 프롬프트가 남길 것을 가리키고 나머지를 마스크로 지웁니다"),
    DE("background (Vorgabe): der Prompt benennt Störendes, alles andere bleibt; "
       "subject: der Prompt benennt, was bleiben soll, alles andere wird "
       "wegmaskiert"),
    FR("background (défaut) : la consigne nomme les gêneurs et tout le reste est "
       "gardé ; subject : la consigne nomme ce qu'il faut garder et tout le "
       "reste est masqué"),
    ES("background (por defecto): la indicación nombra a los estorbos y se "
       "conserva todo lo demás; subject: la indicación nombra lo que hay que "
       "conservar y se tapa todo lo demás"),
    PT("background (padrão): o comando nomeia os estorvos e tudo o resto é "
       "mantido; subject: o comando nomeia o que manter e tudo o resto é "
       "mascarado"),
    IT("background (predefinito): il prompt nomina i disturbi e tutto il resto "
       "resta; subject: il prompt nomina ciò da tenere e tutto il resto viene "
       "mascherato"),
    NL("background (standaard): de prompt noemt de stoorders en al het andere "
       "blijft; subject: de prompt noemt wat blijven moet en al het andere wordt "
       "weggemaskeerd"),
    RU("background (по умолчанию): подсказка называет помехи, всё остальное "
       "сохраняется; subject: подсказка называет то, что надо сохранить, "
       "остальное маскируется"),
    TR("background (varsayılan): istem rahatsız edenleri adlandırır, geri kalan "
       "korunur; subject: istem korunacak olanı adlandırır, geri kalan "
       "maskelenir"));

SS_MSG(xh_mask_out,
    EN("default: 'masks' beside the image directory"),
    JA("既定: 画像ディレクトリの隣の 'masks'"),
    ZH_HANS("默认：图像目录旁边的 'masks'"),
    ZH_HANT("預設：影像目錄旁邊的 'masks'"),
    KO("기본값: 이미지 디렉터리 옆의 'masks'"),
    DE("Vorgabe: 'masks' neben dem Bildverzeichnis"),
    FR("défaut : 'masks' à côté du dossier d'images"),
    ES("por defecto: 'masks' junto a la carpeta de imágenes"),
    PT("padrão: 'masks' ao lado da pasta de imagens"),
    IT("predefinito: 'masks' accanto alla cartella delle immagini"),
    NL("standaard: 'masks' naast de beeldmap"),
    RU("по умолчанию: 'masks' рядом с каталогом изображений"),
    TR("varsayılan: görüntü dizininin yanındaki 'masks'"));

SS_MSG(xh_detect_every,
    EN("run the detector every n frames (default 1); the memory bank carries "
       "instances in between"),
    JA("n フレームごとに検出器を走らせます（既定 1）。その間はメモリバンクが"
       "インスタンスを保ちます"),
    ZH_HANS("每 n 帧运行一次检测器（默认 1）；其间由记忆库维持实例"),
    ZH_HANT("每 n 格執行一次偵測器（預設 1）；其間由記憶庫維持實例"),
    KO("n 프레임마다 검출기를 돌립니다(기본 1). 그 사이는 메모리 뱅크가 인스턴스를 "
       "이어 갑니다"),
    DE("den Detektor alle n Bilder laufen lassen (Vorgabe 1); dazwischen trägt "
       "die Gedächtnisbank die Instanzen"),
    FR("lancer le détecteur toutes les n images (défaut 1) ; entre-temps la "
       "banque de mémoire porte les instances"),
    ES("ejecutar el detector cada n fotogramas (por defecto 1); entre medias el "
       "banco de memoria lleva las instancias"),
    PT("executar o detector a cada n quadros (padrão 1); entretanto o banco de "
       "memória carrega as instâncias"),
    IT("eseguire il rivelatore ogni n fotogrammi (predefinito 1); nel frattempo "
       "la banca di memoria porta le istanze"),
    NL("de detector elke n beelden draaien (standaard 1); daartussen draagt de "
       "geheugenbank de exemplaren"),
    RU("запускать детектор каждые n кадров (по умолчанию 1); в промежутках "
       "экземпляры несёт банк памяти"),
    TR("bulucuyu her n karede bir çalıştır (varsayılan 1); arada örnekleri "
       "bellek bankası taşır"));

SS_MSG(xh_memory_frames,
    EN("cap spatial memory frames per instance; memory attention is linear in "
       "this and dominates the cost"),
    JA("インスタンスごとの空間メモリのフレーム数に上限を設けます。メモリ注意機構は"
       "この数に比例し、コストの大半を占めます"),
    ZH_HANS("限制每个实例的空间记忆帧数；记忆注意力与该数成线性关系，也是开销的大头"),
    ZH_HANT("限制每個實例的空間記憶影格數；記憶注意力與該數成線性關係，也是開銷的大頭"),
    KO("인스턴스마다 공간 메모리 프레임 수에 상한을 둡니다. 메모리 어텐션은 이 수에 "
       "비례하며 비용의 대부분을 차지합니다"),
    DE("räumliche Gedächtnisbilder je Instanz begrenzen; die "
       "Gedächtnisaufmerksamkeit ist darin linear und beherrscht die Kosten"),
    FR("plafonner les images de mémoire spatiale par instance ; l'attention "
       "mémorielle y est linéaire et domine le coût"),
    ES("limitar los fotogramas de memoria espacial por instancia; la atención de "
       "memoria es lineal en esto y domina el coste"),
    PT("limitar os quadros de memória espacial por instância; a atenção de "
       "memória é linear nisto e domina o custo"),
    IT("limitare i fotogrammi di memoria spaziale per istanza; l'attenzione di "
       "memoria è lineare in questo e domina il costo"),
    NL("de ruimtelijke geheugenbeelden per exemplaar begrenzen; de "
       "geheugenaandacht is hierin lineair en beheerst de kosten"),
    RU("ограничить число кадров пространственной памяти на экземпляр; внимание "
       "по памяти линейно по этому числу и определяет основную стоимость"),
    TR("örnek başına uzamsal bellek karesini sınırla; bellek dikkati bunda "
       "doğrusaldır ve maliyete egemendir"));

SS_MSG(xh_max_size,
    EN("longest side handed to the model (default 1600, 0 = off); masks come "
       "back at frame resolution"),
    JA("モデルに渡す長辺（既定 1600、0 で無効）。マスクはフレームの解像度で"
       "返ってきます"),
    ZH_HANS("交给模型的最长边（默认 1600，0 表示关闭）；掩码仍按帧的分辨率返回"),
    ZH_HANT("交給模型的最長邊（預設 1600，0 表示關閉）；遮罩仍按影格的解析度回傳"),
    KO("모델에 넘기는 가장 긴 변(기본 1600, 0 이면 끔). 마스크는 프레임 해상도로 "
       "돌아옵니다"),
    DE("längste Seite, die dem Modell gereicht wird (Vorgabe 1600, 0 = aus); die "
       "Masken kommen in Bildauflösung zurück"),
    FR("plus grand côté remis au modèle (défaut 1600, 0 = désactivé) ; les "
       "masques reviennent à la résolution de l'image"),
    ES("lado más largo que se entrega al modelo (por defecto 1600, 0 = "
       "desactivado); las máscaras vuelven a la resolución del fotograma"),
    PT("maior lado entregue ao modelo (padrão 1600, 0 = desligado); as máscaras "
       "voltam na resolução do quadro"),
    IT("lato più lungo consegnato al modello (predefinito 1600, 0 = disattivo); "
       "le maschere tornano alla risoluzione del fotogramma"),
    NL("langste zijde die aan het model gegeven wordt (standaard 1600, 0 = uit); "
       "de maskers komen terug op beeldresolutie"),
    RU("наибольшая сторона, передаваемая модели (по умолчанию 1600, "
       "0 -- выключено); маски возвращаются в разрешении кадра"),
    TR("modele verilen en uzun kenar (varsayılan 1600, 0 = kapalı); maskeler "
       "kare çözünürlüğünde geri gelir"));

SS_MSG(xh_threshold,
    EN("detection score threshold (default 0.5)"),
    JA("検出スコアのしきい値（既定 0.5）"),
    ZH_HANS("检测得分阈值（默认 0.5）"),
    ZH_HANT("偵測得分閾值（預設 0.5）"),
    KO("검출 점수 임계값(기본 0.5)"),
    DE("Schwelle der Erkennungsbewertung (Vorgabe 0.5)"),
    FR("seuil du score de détection (défaut 0.5)"),
    ES("umbral de la puntuación de detección (por defecto 0.5)"),
    PT("limiar da pontuação de detecção (padrão 0.5)"),
    IT("soglia del punteggio di rilevamento (predefinito 0.5)"),
    NL("drempel van de detectiescore (standaard 0.5)"),
    RU("порог оценки обнаружения (по умолчанию 0.5)"),
    TR("bulma puanı eşiği (varsayılan 0.5)"));

SS_MSG(xh_nms,
    EN("NMS IoU threshold (default 0.1)"),
    JA("NMS の IoU しきい値（既定 0.1）"),
    ZH_HANS("NMS 的 IoU 阈值（默认 0.1）"),
    ZH_HANT("NMS 的 IoU 閾值（預設 0.1）"),
    KO("NMS 의 IoU 임계값(기본 0.1)"),
    DE("IoU-Schwelle der NMS (Vorgabe 0.1)"),
    FR("seuil d'IoU de la NMS (défaut 0.1)"),
    ES("umbral de IoU de la NMS (por defecto 0.1)"),
    PT("limiar de IoU da NMS (padrão 0.1)"),
    IT("soglia di IoU della NMS (predefinito 0.1)"),
    NL("IoU-drempel van de NMS (standaard 0.1)"),
    RU("порог IoU для NMS (по умолчанию 0.1)"),
    TR("NMS IoU eşiği (varsayılan 0.1)"));

SS_MSG(xh_overlay,
    EN("also write a colour overlay next to each mask"),
    JA("各マスクの隣にカラーのオーバーレイも書き出します"),
    ZH_HANS("在每张掩码旁边再写一张彩色叠加图"),
    ZH_HANT("在每張遮罩旁邊再寫一張彩色疊加圖"),
    KO("마스크마다 옆에 컬러 오버레이도 씁니다"),
    DE("neben jeder Maske auch eine farbige Überlagerung schreiben"),
    FR("écrire aussi une superposition en couleur à côté de chaque masque"),
    ES("escribir además una superposición en color junto a cada máscara"),
    PT("escrever também uma sobreposição a cores ao lado de cada máscara"),
    IT("scrivere anche una sovrapposizione a colori accanto a ogni maschera"),
    NL("naast elk masker ook een kleurenoverlay schrijven"),
    RU("рядом с каждой маской записывать ещё и цветное наложение"),
    TR("her maskenin yanına renkli bir bindirme de yaz"));

// ---------------------------------------------------------------------------
// mask
// ---------------------------------------------------------------------------

SS_MSG(cmd_mask,
    EN("Mask the part of every frame that is never scene: a fisheye border, a "
       "watermark, the rig in shot. Needs no model -- it is in the same place "
       "in every frame, so it is a shape, not an object."),
    JA("どのフレームでも風景ではない部分にマスクをかけます。魚眼の黒枠、透かし、"
       "写り込んだ機材など。モデルは不要です。位置がどのフレームでも同じなので、"
       "物体ではなく図形として扱えます。"),
    ZH_HANS("为每一帧中始终不是景物的部分做掩码：鱼眼黑边、水印、入镜的器材。"
            "不需要模型——它在每一帧的位置都相同，所以是图形，不是物体。"),
    ZH_HANT("為每一格中始終不是景物的部分做遮罩：魚眼黑邊、浮水印、入鏡的器材。"
            "不需要模型——它在每一格的位置都相同，所以是圖形，不是物體。"),
    KO("어느 프레임에서도 장면이 아닌 부분을 가립니다. 어안 검은 테두리, "
       "워터마크, 화면에 든 장비 같은 것들입니다. 모델은 필요 없습니다. "
       "자리가 프레임마다 같으니 물체가 아니라 도형입니다."),
    DE("Den Teil jedes Bildes maskieren, der nie Szene ist: ein Fisheye-Rand, "
       "ein Wasserzeichen, das Stativ im Bild. Ohne Modell -- er steht in "
       "jedem Bild an derselben Stelle, ist also eine Form, kein Objekt."),
    FR("Masquer la part de chaque image qui n'est jamais la scène : bord de "
       "fisheye, filigrane, matériel dans le champ. Sans modèle : c'est au "
       "même endroit sur chaque image, donc une forme, pas un objet."),
    ES("Enmascarar la parte de cada fotograma que nunca es la escena: el borde "
       "de ojo de pez, una marca de agua, el equipo en cuadro. Sin modelo: "
       "está en el mismo sitio en todos, así que es una forma, no un objeto."),
    PT("Mascarar a parte de cada quadro que nunca é a cena: a borda de "
       "olho-de-peixe, uma marca d'água, o equipamento em quadro. Sem modelo: "
       "fica no mesmo lugar em todos, então é uma forma, não um objeto."),
    IT("Mascherare la parte di ogni fotogramma che non è mai la scena: il "
       "bordo fisheye, una filigrana, l'attrezzatura in campo. Senza modello: "
       "sta sempre nello stesso punto, quindi è una forma, non un oggetto."),
    NL("Het deel van elk beeld maskeren dat nooit scène is: een fisheye-rand, "
       "een watermerk, de statiefkop in beeld. Zonder model -- het staat in "
       "elk beeld op dezelfde plek, dus het is een vorm, geen object."),
    RU("Замаскировать ту часть каждого кадра, которая никогда не является "
       "сценой: чёрный край фишая, водяной знак, штатив в кадре. Модель не "
       "нужна: место одно и то же во всех кадрах, так что это фигура, а не "
       "объект."),
    TR("Her karede sahne olmayan bölümü maskele: balıkgözü kenarı, filigran, "
       "kadrajdaki ekipman. Model gerekmez -- her karede aynı yerde durur, "
       "yani nesne değil, biçimdir."));

SS_MSG(mh_out,
    EN("where the masks go (default: 'masks' beside the image directory)"),
    JA("マスクの書き出し先（既定: 画像ディレクトリの隣の 'masks'）"),
    ZH_HANS("掩码写到哪里（默认：图像目录旁边的 'masks'）"),
    ZH_HANT("遮罩寫到哪裡（預設：影像目錄旁邊的 'masks'）"),
    KO("마스크를 쓸 곳(기본값: 이미지 디렉터리 옆의 'masks')"),
    DE("wohin die Masken kommen (Vorgabe: 'masks' neben dem Bildverzeichnis)"),
    FR("où vont les masques (défaut : 'masks' à côté du dossier d'images)"),
    ES("dónde van las máscaras (por defecto: 'masks' junto a las imágenes)"),
    PT("para onde vão as máscaras (padrão: 'masks' ao lado das imagens)"),
    IT("dove vanno le maschere (predefinito: 'masks' accanto alle immagini)"),
    NL("waar de maskers heen gaan (standaard: 'masks' naast de beeldmap)"),
    RU("куда класть маски (по умолчанию: 'masks' рядом с изображениями)"),
    TR("maskelerin gideceği yer (varsayılan: görüntülerin yanındaki 'masks')"));

SS_MSG(mh_shape,
    EN("use these shapes instead of looking for a border; ';' separates them "
       "and a leading '-' cuts one out again"),
    JA("枠を探す代わりにこの図形を使います。';' で区切り、先頭の '-' はその"
       "図形の内側を逆に取り除きます"),
    ZH_HANS("用这些图形，而不是去找边框；用 ';' 分隔，开头的 '-' 表示反过来去掉"
            "该图形内部"),
    ZH_HANT("用這些圖形，而不是去找邊框；用 ';' 分隔，開頭的 '-' 表示反過來去掉"
            "該圖形內部"),
    KO("테두리를 찾는 대신 이 도형을 씁니다. ';' 로 나누고, 앞의 '-' 는 그 "
       "도형 안쪽을 도로 없앱니다"),
    DE("diese Formen benutzen statt einen Rand zu suchen; ';' trennt sie, ein "
       "vorangestelltes '-' schneidet eine wieder heraus"),
    FR("utiliser ces formes au lieu de chercher un bord ; ';' les sépare, un "
       "'-' en tête en retire une au contraire"),
    ES("usar estas formas en vez de buscar un borde; ';' las separa y un '-' "
       "delante recorta una en lugar de conservarla"),
    PT("usar estas formas em vez de procurar uma borda; ';' as separa e um '-' "
       "à frente recorta uma em vez de mantê-la"),
    IT("usare queste forme invece di cercare un bordo; ';' le separa e un '-' "
       "davanti ne ritaglia una invece di tenerla"),
    NL("deze vormen gebruiken in plaats van een rand te zoeken; ';' scheidt "
       "ze, een '-' ervoor snijdt er juist een weg"),
    RU("взять эти фигуры вместо поиска края; ';' разделяет их, а '-' в начале "
       "наоборот вырезает фигуру"),
    TR("kenar aramak yerine bu biçimleri kullan; ';' ayırır, baştaki '-' ise "
       "biçimin içini tersine keser"));

SS_MSG(mh_shrink,
    EN("pull the found boundary inwards, as a fraction of its radius "
       "(default 0.01); the outermost pixels of a lens circle are dim and "
       "smeared, and worth losing"),
    JA("見つけた境界を半径に対する割合だけ内側に寄せます（既定 0.01）。"
       "レンズ円のいちばん外側の画素は暗くにじんでおり、捨ててよい部分です"),
    ZH_HANS("把找到的边界按半径的比例往里收（默认 0.01）；镜头圆最外圈的像素又"
            "暗又糊，丢掉更好"),
    ZH_HANT("把找到的邊界按半徑的比例往裡收（預設 0.01）；鏡頭圓最外圈的像素又"
            "暗又糊，丟掉更好"),
    KO("찾은 경계를 반지름 대비 비율만큼 안쪽으로 당깁니다(기본값 0.01). "
       "렌즈 원의 가장 바깥 화소는 어둡고 번져 있어 버리는 편이 낫습니다"),
    DE("die gefundene Grenze nach innen ziehen, als Bruchteil ihres Radius "
       "(Vorgabe 0,01); die äußersten Pixel eines Objektivkreises sind dunkel "
       "und verschmiert und dürfen weg"),
    FR("resserrer la limite trouvée, en fraction de son rayon (défaut 0,01) ; "
       "les pixels du bord d'un cercle-image sont sombres et étalés, autant "
       "les perdre"),
    ES("meter hacia dentro el límite hallado, como fracción de su radio (por "
       "defecto 0,01); los píxeles del borde del círculo son oscuros y "
       "emborronados, y conviene perderlos"),
    PT("puxar para dentro o limite achado, como fração do raio (padrão 0,01); "
       "os pixels da borda do círculo são escuros e borrados, e é melhor "
       "perdê-los"),
    IT("stringere il confine trovato, come frazione del raggio (predefinito "
       "0,01); i pixel più esterni del cerchio dell'obiettivo sono scuri e "
       "sbavati, meglio perderli"),
    NL("de gevonden grens naar binnen halen, als fractie van de straal "
       "(standaard 0,01); de buitenste pixels van een lenscirkel zijn donker "
       "en uitgesmeerd, en mogen weg"),
    RU("подтянуть найденную границу внутрь, долей от её радиуса (по умолчанию "
       "0,01): крайние пиксели круга изображения тусклые и смазанные, их не "
       "жалко"),
    TR("bulunan sınırı yarıçapının bir kesri kadar içeri çek (varsayılan "
       "0,01); mercek dairesinin en dış pikselleri sönük ve bulaşıktır, "
       "gitmeleri iyidir"));

SS_MSG(mh_samples,
    EN("frames read per camera when looking for the border (default 24)"),
    JA("枠を探すときにカメラごとに読むフレーム数（既定 24）"),
    ZH_HANS("找边框时每台相机读取的帧数（默认 24）"),
    ZH_HANT("找邊框時每台相機讀取的影格數（預設 24）"),
    KO("테두리를 찾을 때 카메라마다 읽는 프레임 수(기본값 24)"),
    DE("Bilder je Kamera, die für die Randsuche gelesen werden (Vorgabe 24)"),
    FR("images lues par caméra pour chercher le bord (défaut 24)"),
    ES("fotogramas leídos por cámara al buscar el borde (por defecto 24)"),
    PT("quadros lidos por câmera ao procurar a borda (padrão 24)"),
    IT("fotogrammi letti per fotocamera nella ricerca del bordo (predefinito 24)"),
    NL("beelden per camera die voor de randzoektocht worden gelezen (standaard 24)"),
    RU("сколько кадров с камеры читать при поиске края (по умолчанию 24)"),
    TR("kenar aranırken her kameradan okunan kare sayısı (varsayılan 24)"));

SS_MSG(mh_dark,
    EN("brightness at or below which a pixel counts as black, 0..255 "
       "(default 16)"),
    JA("この明るさ以下の画素を黒とみなします。0..255（既定 16）"),
    ZH_HANS("亮度不高于此值的像素算作黑色，0..255（默认 16）"),
    ZH_HANT("亮度不高於此值的像素算作黑色，0..255（預設 16）"),
    KO("이 밝기 이하의 화소를 검정으로 봅니다. 0..255(기본값 16)"),
    DE("Helligkeit, bis zu der ein Pixel als schwarz gilt, 0..255 (Vorgabe 16)"),
    FR("luminosité jusqu'à laquelle un pixel compte comme noir, 0..255 "
       "(défaut 16)"),
    ES("brillo hasta el que un píxel cuenta como negro, 0..255 (por defecto 16)"),
    PT("brilho até o qual um pixel conta como preto, 0..255 (padrão 16)"),
    IT("luminosità fino a cui un pixel conta come nero, 0..255 (predefinito 16)"),
    NL("helderheid tot waar een pixel als zwart telt, 0..255 (standaard 16)"),
    RU("яркость, до которой пиксель считается чёрным, 0..255 (по умолчанию 16)"),
    TR("bir pikselin siyah sayıldığı üst parlaklık, 0..255 (varsayılan 16)"));

SS_MSG(mh_image,
    EN("also drop wherever this image is black -- the way out when no ellipse "
       "or rectangle describes the region"),
    JA("この画像が黒い場所も併せて取り除きます。楕円や矩形で表せない領域の"
       "逃げ道です"),
    ZH_HANS("这张图为黑的地方也一并去掉——椭圆和矩形描述不了的区域走这条路"),
    ZH_HANT("這張圖為黑的地方也一併去掉——橢圓和矩形描述不了的區域走這條路"),
    KO("이 이미지가 검은 곳도 함께 없앱니다. 타원이나 사각형으로 못 그리는 "
       "영역을 위한 통로입니다"),
    DE("auch dort verwerfen, wo dieses Bild schwarz ist -- der Ausweg, wenn "
       "weder Ellipse noch Rechteck den Bereich beschreibt"),
    FR("retirer aussi là où cette image est noire -- la porte de sortie quand "
       "ni ellipse ni rectangle ne décrit la zone"),
    ES("quitar además donde esta imagen sea negra: la salida cuando ni una "
       "elipse ni un rectángulo describen la zona"),
    PT("tirar também onde esta imagem for preta -- a saída quando nem elipse "
       "nem retângulo descrevem a região"),
    IT("togliere anche dove questa immagine è nera -- la via d'uscita quando "
       "né un'ellisse né un rettangolo descrivono la zona"),
    NL("ook weghalen waar dit beeld zwart is -- de uitweg als geen ellips of "
       "rechthoek het gebied beschrijft"),
    RU("убрать ещё и там, где это изображение чёрное: выход, когда область не "
       "описать ни эллипсом, ни прямоугольником"),
    TR("bu görüntünün siyah olduğu yerleri de at -- bölgeyi elips ya da "
       "dikdörtgen anlatamadığında çıkış yolu"));

SS_MSG(mh_print,
    EN("print the shapes and write nothing"),
    JA("図形を表示するだけで、何も書き出しません"),
    ZH_HANS("只打印图形，不写出任何文件"),
    ZH_HANT("只印出圖形，不寫出任何檔案"),
    KO("도형만 보이고 아무것도 쓰지 않습니다"),
    DE("die Formen ausgeben und nichts schreiben"),
    FR("afficher les formes sans rien écrire"),
    ES("imprimir las formas y no escribir nada"),
    PT("imprimir as formas e não escrever nada"),
    IT("stampare le forme senza scrivere nulla"),
    NL("de vormen tonen en niets schrijven"),
    RU("вывести фигуры и ничего не записывать"),
    TR("biçimleri yazdır, hiçbir şey yazma"));

SS_MSG(mh_preview,
    EN("write one frame with the mask drawn over it, to check by eye"),
    JA("目で確かめられるよう、マスクを重ねたフレームを 1 枚書き出します"),
    ZH_HANS("写出一帧并叠上掩码，便于用眼睛核对"),
    ZH_HANT("寫出一格並疊上遮罩，便於用眼睛核對"),
    KO("눈으로 확인하도록 마스크를 덧그린 프레임 한 장을 씁니다"),
    DE("ein Bild mit darübergelegter Maske schreiben, zum Prüfen mit dem Auge"),
    FR("écrire une image avec le masque dessus, pour vérifier à l'œil"),
    ES("escribir un fotograma con la máscara encima, para comprobarlo a ojo"),
    PT("escrever um quadro com a máscara por cima, para conferir a olho"),
    IT("scrivere un fotogramma con la maschera sopra, da controllare a occhio"),
    NL("één beeld met het masker eroverheen schrijven, om met het oog te controleren"),
    RU("записать один кадр с наложенной маской, чтобы проверить глазом"),
    TR("gözle bakmak için maskesi üstüne çizilmiş bir kare yaz"));

SS_MSG(mh_replace,
    EN("overwrite masks already in the output folder; without it they are "
       "intersected, which is how this stacks on top of a model's masks"),
    JA("出力先にあるマスクを上書きします。付けない場合は積を取り、モデルの"
       "マスクの上に重ねられます"),
    ZH_HANS("覆盖输出目录中已有的掩码；不加时取交集，正是靠这一点叠在模型掩码之上"),
    ZH_HANT("覆蓋輸出目錄中已有的遮罩；不加時取交集，正是靠這一點疊在模型遮罩之上"),
    KO("출력 폴더에 이미 있는 마스크를 덮어씁니다. 없으면 교집합을 취하며, "
       "그렇게 모델 마스크 위에 겹칩니다"),
    DE("Masken im Ausgabeordner überschreiben; ohne das werden sie geschnitten, "
       "und genau so legt sich dies über die Masken eines Modells"),
    FR("écraser les masques déjà dans le dossier de sortie ; sans cela ils sont "
       "intersectés, et c'est ainsi que ceci se pose sur ceux d'un modèle"),
    ES("sobrescribir las máscaras que ya estén en la carpeta de salida; sin "
       "esto se intersecan, que es como esto se apila sobre las de un modelo"),
    PT("sobrescrever as máscaras já na pasta de saída; sem isso elas são "
       "interseccionadas, que é como isto se empilha sobre as de um modelo"),
    IT("sovrascrivere le maschere già nella cartella di uscita; senza, vengono "
       "intersecate, ed è così che questo si somma a quelle di un modello"),
    NL("maskers die al in de uitvoermap staan overschrijven; zonder dit worden "
       "ze doorsneden, en zo stapelt dit op die van een model"),
    RU("перезаписать маски, уже лежащие в выходной папке; без этого берётся их "
       "пересечение — так это и ложится поверх масок модели"),
    TR("çıktı klasöründeki maskeleri üzerine yaz; bu olmadan kesişimleri "
       "alınır, bu da modelin maskelerinin üstüne böyle biner"));

}  // namespace samhelp
}  // namespace msg
}  // namespace i18n
}  // namespace spirula

#include "i18n/EndCatalog.h"
