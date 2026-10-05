#pragma once

// The mask editor (app/gui/mask/): its tools' modes, its status strip, its
// errors and the two log lines the dataset run prints for it. Same two rules
// as Edit.h: counts are labelled, never inflected; keys stay Latin.

#include "i18n/BeginCatalog.h"

namespace spirula {
namespace i18n {
namespace msg {
namespace maskedit {

// ===========================================================================
// History labels
// ===========================================================================

SS_MSG(op_drop,
    EN("Force drop"),
    JA("強制除外"),
    ZH_HANS("强制丢弃"),
    ZH_HANT("強制捨棄"),
    KO("강제 제외"),
    DE("Erzwungen verwerfen"),
    FR("Exclusion forcée"),
    ES("Descarte forzado"),
    PT("Descarte forçado"),
    IT("Scarto forzato"),
    NL("Geforceerd weglaten"),
    RU("Принудительно убрать"),
    TR("Zorla at"));

SS_MSG(op_keep,
    EN("Force keep"),
    JA("強制保持"),
    ZH_HANS("强制保留"),
    ZH_HANT("強制保留"),
    KO("강제 유지"),
    DE("Erzwungen behalten"),
    FR("Conservation forcée"),
    ES("Conservación forzada"),
    PT("Manutenção forçada"),
    IT("Mantenimento forzato"),
    NL("Geforceerd behouden"),
    RU("Принудительно оставить"),
    TR("Zorla tut"));

SS_MSG(op_clear,
    EN("Clear correction"),
    JA("修正を消去"),
    ZH_HANS("清除修正"),
    ZH_HANT("清除修正"),
    KO("수정 지우기"),
    DE("Korrektur löschen"),
    FR("Effacer la correction"),
    ES("Borrar corrección"),
    PT("Apagar correção"),
    IT("Cancella correzione"),
    NL("Correctie wissen"),
    RU("Стереть исправление"),
    TR("Düzeltmeyi sil"));

// ===========================================================================
// Entry, window, actions
// ===========================================================================

SS_MSG(correct_masks,
    EN("Correct Masks"),
    JA("マスクを修正"),
    ZH_HANS("修正蒙版"),
    ZH_HANT("修正遮罩"),
    KO("마스크 수정"),
    DE("Masken korrigieren"),
    FR("Corriger les masques"),
    ES("Corregir máscaras"),
    PT("Corrigir máscaras"),
    IT("Correggi maschere"),
    NL("Maskers corrigeren"),
    RU("Исправить маски"),
    TR("Maskeleri düzelt"));

SS_MSG(correct_masks_help,
    EN("Paint corrections onto the masks a run wrote. They are kept in mask_edits/ beside the dataset and re-applied when masking runs again."),
    JA("実行が書き出したマスクに修正を描き込みます。修正はデータセット横の mask_edits/ に保存され、マスク処理を再実行しても再適用されます。"),
    ZH_HANS("在运行生成的蒙版上绘制修正。修正保存在数据集旁的 mask_edits/ 中，重新运行蒙版处理时会再次应用。"),
    ZH_HANT("在執行產生的遮罩上繪製修正。修正保存在資料集旁的 mask_edits/ 中，重新執行遮罩處理時會再次套用。"),
    KO("실행이 기록한 마스크 위에 수정을 칠합니다. 수정은 데이터셋 옆 mask_edits/에 보관되며 마스킹을 다시 실행해도 다시 적용됩니다."),
    DE("Korrekturen auf die Masken malen, die ein Lauf geschrieben hat. Sie liegen in mask_edits/ neben dem Datensatz und werden nach einem erneuten Maskieren wieder angewendet."),
    FR("Peindre des corrections sur les masques écrits par une exécution. Elles sont conservées dans mask_edits/ à côté du jeu de données et réappliquées quand le masquage est relancé."),
    ES("Pinta correcciones sobre las máscaras que escribió una ejecución. Se guardan en mask_edits/ junto al conjunto de datos y se vuelven a aplicar cuando se repite el enmascarado."),
    PT("Pinte correções sobre as máscaras que uma execução escreveu. Elas ficam em mask_edits/ ao lado do conjunto de dados e são reaplicadas quando o mascaramento é executado de novo."),
    IT("Dipingi correzioni sulle maschere scritte da un'esecuzione. Sono conservate in mask_edits/ accanto al dataset e riapplicate quando il mascheramento viene rieseguito."),
    NL("Schilder correcties op de maskers die een run heeft geschreven. Ze staan in mask_edits/ naast de dataset en worden opnieuw toegepast als het maskeren opnieuw draait."),
    RU("Нанесите исправления на маски, записанные прогоном. Они хранятся в mask_edits/ рядом с набором данных и применяются заново при повторном маскировании."),
    TR("Bir çalıştırmanın yazdığı maskelere düzeltmeler çizin. Düzeltmeler veri kümesinin yanındaki mask_edits/ içinde tutulur ve maskeleme yeniden çalıştığında yeniden uygulanır."));

SS_MSG(window_title,
    EN("Mask correction"),
    JA("マスク修正"),
    ZH_HANS("蒙版修正"),
    ZH_HANT("遮罩修正"),
    KO("마스크 수정"),
    DE("Maskenkorrektur"),
    FR("Correction des masques"),
    ES("Corrección de máscaras"),
    PT("Correção de máscaras"),
    IT("Correzione maschere"),
    NL("Maskercorrectie"),
    RU("Исправление масок"),
    TR("Maske düzeltme"));

SS_MSG(working,
    EN("Loading the frame..."),
    JA("フレームを読み込み中…"),
    ZH_HANS("正在加载帧…"),
    ZH_HANT("正在載入影格…"),
    KO("프레임을 불러오는 중…"),
    DE("Bild wird geladen …"),
    FR("Chargement de l'image…"),
    ES("Cargando el fotograma…"),
    PT("Carregando o quadro…"),
    IT("Caricamento del fotogramma…"),
    NL("Frame wordt geladen…"),
    RU("Загрузка кадра…"),
    TR("Kare yükleniyor…"));

SS_MSG(save,
    EN("Save"),
    JA("保存"),
    ZH_HANS("保存"),
    ZH_HANT("儲存"),
    KO("저장"),
    DE("Speichern"),
    FR("Enregistrer"),
    ES("Guardar"),
    PT("Salvar"),
    IT("Salva"),
    NL("Opslaan"),
    RU("Сохранить"),
    TR("Kaydet"));

SS_MSG(save_help,
    EN("Write the corrected mask into masks/ and the correction layers into mask_edits/. This also happens when you change frame or close the window."),
    JA("修正済みマスクを masks/ に、修正レイヤーを mask_edits/ に書き出します。フレームを切り替えたときやウィンドウを閉じたときにも自動で行われます。"),
    ZH_HANS("将修正后的蒙版写入 masks/，将修正图层写入 mask_edits/。切换帧或关闭窗口时也会自动执行。"),
    ZH_HANT("將修正後的遮罩寫入 masks/，將修正圖層寫入 mask_edits/。切換影格或關閉視窗時也會自動執行。"),
    KO("수정된 마스크를 masks/에, 수정 레이어를 mask_edits/에 기록합니다. 프레임을 바꾸거나 창을 닫을 때도 자동으로 수행됩니다."),
    DE("Die korrigierte Maske nach masks/ und die Korrekturebenen nach mask_edits/ schreiben. Geschieht auch beim Bildwechsel und beim Schließen des Fensters."),
    FR("Écrit le masque corrigé dans masks/ et les calques de correction dans mask_edits/. Se fait aussi au changement d'image et à la fermeture de la fenêtre."),
    ES("Escribe la máscara corregida en masks/ y las capas de corrección en mask_edits/. También ocurre al cambiar de fotograma o cerrar la ventana."),
    PT("Escreve a máscara corrigida em masks/ e as camadas de correção em mask_edits/. Também acontece ao mudar de quadro ou fechar a janela."),
    IT("Scrive la maschera corretta in masks/ e i livelli di correzione in mask_edits/. Avviene anche quando cambi fotogramma o chiudi la finestra."),
    NL("Schrijft het gecorrigeerde masker naar masks/ en de correctielagen naar mask_edits/. Gebeurt ook bij het wisselen van frame en het sluiten van het venster."),
    RU("Записывает исправленную маску в masks/, а слои исправлений в mask_edits/. Происходит также при смене кадра и закрытии окна."),
    TR("Düzeltilmiş maskeyi masks/ içine, düzeltme katmanlarını mask_edits/ içine yazar. Kare değiştirildiğinde ve pencere kapatıldığında da yapılır."));

SS_MSG(revert_frame,
    EN("Revert frame"),
    JA("フレームを元に戻す"),
    ZH_HANS("还原此帧"),
    ZH_HANT("還原此影格"),
    KO("프레임 되돌리기"),
    DE("Bild zurücksetzen"),
    FR("Restaurer l'image"),
    ES("Restablecer fotograma"),
    PT("Repor quadro"),
    IT("Ripristina fotogramma"),
    NL("Frame terugzetten"),
    RU("Вернуть кадр"),
    TR("Kareyi geri al"));

SS_MSG(revert_frame_help,
    EN("Put the mask back exactly as the run wrote it and delete this frame's corrections."),
    JA("マスクを実行が書き出した状態に完全に戻し、このフレームの修正を削除します。"),
    ZH_HANS("将蒙版恢复为运行写出时的原样，并删除此帧的修正。"),
    ZH_HANT("將遮罩恢復為執行寫出時的原樣，並刪除此影格的修正。"),
    KO("마스크를 실행이 기록한 그대로 되돌리고 이 프레임의 수정을 삭제합니다."),
    DE("Die Maske genau so wiederherstellen, wie der Lauf sie geschrieben hat, und die Korrekturen dieses Bildes löschen."),
    FR("Remet le masque exactement tel que l'exécution l'a écrit et supprime les corrections de cette image."),
    ES("Devuelve la máscara exactamente a como la escribió la ejecución y elimina las correcciones de este fotograma."),
    PT("Repõe a máscara exatamente como a execução a escreveu e apaga as correções deste quadro."),
    IT("Riporta la maschera esattamente a come l'esecuzione l'ha scritta ed elimina le correzioni di questo fotogramma."),
    NL("Zet het masker precies terug zoals de run het schreef en verwijdert de correcties van dit frame."),
    RU("Возвращает маску ровно в том виде, в каком её записал прогон, и удаляет исправления этого кадра."),
    TR("Maskeyi çalıştırmanın yazdığı haline tam olarak geri döndürür ve bu karenin düzeltmelerini siler."));

SS_MSG(revert_all,
    EN("Revert all"),
    JA("すべて元に戻す"),
    ZH_HANS("全部还原"),
    ZH_HANT("全部還原"),
    KO("모두 되돌리기"),
    DE("Alle zurücksetzen"),
    FR("Tout restaurer"),
    ES("Restablecer todo"),
    PT("Repor tudo"),
    IT("Ripristina tutto"),
    NL("Alles terugzetten"),
    RU("Вернуть все"),
    TR("Tümünü geri al"));

SS_MSG(revert_all_help,
    EN("Delete every hand correction in this dataset and put each mask back exactly as the run wrote it. Asks first; this cannot be undone."),
    JA("このデータセットの手動修正をすべて削除し、各マスクを実行が書き出した状態に完全に戻します。実行前に確認します。元に戻すことはできません。"),
    ZH_HANS("删除此数据集中的所有手动修正，并将每个蒙版完全恢复为运行写出时的原样。执行前会先确认；此操作无法撤销。"),
    ZH_HANT("刪除此資料集中的所有手動修正，並將每個遮罩完全恢復為執行寫出時的原樣。執行前會先確認；此操作無法復原。"),
    KO("이 데이터셋의 모든 수동 수정을 삭제하고 각 마스크를 실행이 기록한 그대로 되돌립니다. 먼저 확인을 받으며, 되돌릴 수 없습니다."),
    DE("Alle Handkorrekturen dieses Datensatzes löschen und jede Maske genau so wiederherstellen, wie der Lauf sie geschrieben hat. Fragt vorher nach; lässt sich nicht rückgängig machen."),
    FR("Supprime toutes les corrections manuelles de ce jeu de données et remet chaque masque exactement tel que l'exécution l'a écrit. Demande d'abord confirmation ; irréversible."),
    ES("Elimina todas las correcciones manuales de este conjunto de datos y devuelve cada máscara exactamente a como la escribió la ejecución. Pide confirmación antes; no se puede deshacer."),
    PT("Apaga todas as correções manuais deste conjunto de dados e repõe cada máscara exatamente como a execução a escreveu. Pede confirmação primeiro; não pode ser anulado."),
    IT("Elimina tutte le correzioni manuali di questo dataset e riporta ogni maschera esattamente a come l'esecuzione l'ha scritta. Chiede prima conferma; non si può annullare."),
    NL("Verwijdert alle handmatige correcties van deze dataset en zet elk masker precies terug zoals de run het schreef. Vraagt eerst om bevestiging; kan niet ongedaan worden gemaakt."),
    RU("Удаляет все ручные исправления этого набора данных и возвращает каждую маску ровно в том виде, в каком её записал прогон. Сначала спрашивает; отменить нельзя."),
    TR("Bu veri kümesindeki tüm elle yapılmış düzeltmeleri siler ve her maskeyi çalıştırmanın yazdığı haline tam olarak geri döndürür. Önce onay ister; geri alınamaz."));

SS_MSG(revert_all_title,
    EN("Delete every correction?"),
    JA("すべての修正を削除しますか？"),
    ZH_HANS("删除所有修正？"),
    ZH_HANT("刪除所有修正？"),
    KO("모든 수정을 삭제할까요?"),
    DE("Alle Korrekturen löschen?"),
    FR("Supprimer toutes les corrections ?"),
    ES("¿Eliminar todas las correcciones?"),
    PT("Apagar todas as correções?"),
    IT("Eliminare tutte le correzioni?"),
    NL("Alle correcties verwijderen?"),
    RU("Удалить все исправления?"),
    TR("Tüm düzeltmeler silinsin mi?"));

SS_MSG(revert_all_confirm,
    EN("Every hand correction in this dataset is deleted, and each corrected mask goes back exactly as the run wrote it. This cannot be undone."),
    JA("このデータセットの手動修正はすべて削除され、修正済みの各マスクは実行が書き出した状態に完全に戻ります。この操作は元に戻せません。"),
    ZH_HANS("此数据集中的所有手动修正都将被删除，每个已修正的蒙版都会完全恢复为运行写出时的原样。此操作无法撤销。"),
    ZH_HANT("此資料集中的所有手動修正都將被刪除，每個已修正的遮罩都會完全恢復為執行寫出時的原樣。此操作無法復原。"),
    KO("이 데이터셋의 모든 수동 수정이 삭제되고, 수정된 각 마스크는 실행이 기록한 그대로 돌아갑니다. 이 작업은 되돌릴 수 없습니다."),
    DE("Alle Handkorrekturen dieses Datensatzes werden gelöscht, und jede korrigierte Maske wird genau so wiederhergestellt, wie der Lauf sie geschrieben hat. Das lässt sich nicht rückgängig machen."),
    FR("Toutes les corrections manuelles de ce jeu de données sont supprimées, et chaque masque corrigé redevient exactement tel que l'exécution l'a écrit. Cette action est irréversible."),
    ES("Se eliminan todas las correcciones manuales de este conjunto de datos y cada máscara corregida vuelve exactamente a como la escribió la ejecución. No se puede deshacer."),
    PT("Todas as correções manuais deste conjunto de dados são apagadas e cada máscara corrigida volta exatamente ao que a execução escreveu. Não pode ser anulado."),
    IT("Tutte le correzioni manuali di questo dataset vengono eliminate e ogni maschera corretta torna esattamente a come l'esecuzione l'ha scritta. Non si può annullare."),
    NL("Alle handmatige correcties van deze dataset worden verwijderd en elk gecorrigeerd masker gaat precies terug naar hoe de run het schreef. Dit kan niet ongedaan worden gemaakt."),
    RU("Все ручные исправления этого набора данных будут удалены, а каждая исправленная маска вернётся ровно в тот вид, в каком её записал прогон. Отменить это нельзя."),
    TR("Bu veri kümesindeki tüm elle yapılmış düzeltmeler silinir ve düzeltilmiş her maske çalıştırmanın yazdığı haline tam olarak döner. Bu işlem geri alınamaz."));

SS_MSG(revert_all_unsaved,
    EN("The open frame's unsaved changes are discarded as well."),
    JA("開いているフレームの未保存の変更も破棄されます。"),
    ZH_HANS("当前帧未保存的更改也将被丢弃。"),
    ZH_HANT("目前影格未儲存的變更也將被捨棄。"),
    KO("열려 있는 프레임의 저장되지 않은 변경 사항도 버려집니다."),
    DE("Nicht gespeicherte Änderungen am offenen Bild werden ebenfalls verworfen."),
    FR("Les modifications non enregistrées de l'image ouverte sont aussi perdues."),
    ES("También se descartan los cambios sin guardar del fotograma abierto."),
    PT("As alterações não guardadas do quadro aberto também são descartadas."),
    IT("Anche le modifiche non salvate del fotogramma aperto vengono scartate."),
    NL("Niet-opgeslagen wijzigingen in het open frame worden ook weggegooid."),
    RU("Несохранённые изменения открытого кадра тоже будут отброшены."),
    TR("Açık karenin kaydedilmemiş değişiklikleri de atılır."));

SS_MSG(revert_all_button,
    EN("Delete corrections"),
    JA("修正を削除"),
    ZH_HANS("删除修正"),
    ZH_HANT("刪除修正"),
    KO("수정 삭제"),
    DE("Korrekturen löschen"),
    FR("Supprimer les corrections"),
    ES("Eliminar correcciones"),
    PT("Apagar correções"),
    IT("Elimina correzioni"),
    NL("Correcties verwijderen"),
    RU("Удалить исправления"),
    TR("Düzeltmeleri sil"));

SS_MSG(undo,
    EN("Undo"),
    JA("元に戻す"),
    ZH_HANS("撤销"),
    ZH_HANT("復原"),
    KO("실행 취소"),
    DE("Rückgängig"),
    FR("Annuler"),
    ES("Deshacer"),
    PT("Desfazer"),
    IT("Annulla"),
    NL("Ongedaan maken"),
    RU("Отменить"),
    TR("Geri al"));

SS_MSG(redo,
    EN("Redo"),
    JA("やり直し"),
    ZH_HANS("重做"),
    ZH_HANT("重做"),
    KO("다시 실행"),
    DE("Wiederholen"),
    FR("Rétablir"),
    ES("Rehacer"),
    PT("Refazer"),
    IT("Ripeti"),
    NL("Opnieuw"),
    RU("Вернуть"),
    TR("Yinele"));

SS_MSG(done,
    EN("Done"),
    JA("完了"),
    ZH_HANS("完成"),
    ZH_HANT("完成"),
    KO("완료"),
    DE("Fertig"),
    FR("Terminé"),
    ES("Listo"),
    PT("Concluído"),
    IT("Fatto"),
    NL("Klaar"),
    RU("Готово"),
    TR("Bitti"));

SS_MSG(done_help,
    EN("Save the open frame and close the editor."),
    JA("開いているフレームを保存して、エディターを閉じます。"),
    ZH_HANS("保存当前帧并关闭编辑器。"),
    ZH_HANT("儲存目前的影格並關閉編輯器。"),
    KO("열려 있는 프레임을 저장하고 편집기를 닫습니다."),
    DE("Das offene Bild speichern und den Editor schließen."),
    FR("Enregistrer l'image ouverte et fermer l'éditeur."),
    ES("Guardar el fotograma abierto y cerrar el editor."),
    PT("Salvar o quadro aberto e fechar o editor."),
    IT("Salva il fotogramma aperto e chiudi l'editor."),
    NL("Het open beeld opslaan en de editor sluiten."),
    RU("Сохранить открытый кадр и закрыть редактор."),
    TR("Açık kareyi kaydet ve düzenleyiciyi kapat."));

SS_MSG(tool_eraser,
    EN("Eraser"),     JA("消しゴム"),   ZH_HANS("橡皮擦"), ZH_HANT("橡皮擦"),
    KO("지우개"),      DE("Radierer"),   FR("Gomme"),
    ES("Borrador"),   PT("Borracha"),   IT("Gomma"),
    NL("Gum"),        RU("Ластик"),     TR("Silgi"));

SS_MSG(mode_add,
    EN("Add"),         JA("追加"),       ZH_HANS("添加"),   ZH_HANT("新增"),
    KO("추가"),        DE("Hinzufügen"), FR("Ajouter"),
    ES("Añadir"),      PT("Adicionar"),  IT("Aggiungi"),
    NL("Toevoegen"),   RU("Добавить"),   TR("Ekle"));

SS_MSG(mode_subtract,
    EN("Subtract"),    JA("削除"),       ZH_HANS("减去"),   ZH_HANT("減去"),
    KO("빼기"),        DE("Abziehen"),   FR("Soustraire"),
    ES("Restar"),      PT("Subtrair"),   IT("Sottrai"),
    NL("Aftrekken"),   RU("Вычесть"),    TR("Çıkar"));

SS_MSG(mode_help,
    EN("What a plain drag or click does. Add grows the masked (removed) area; Subtract takes it back and keeps those pixels. Ctrl does the other one."),
    JA("通常のドラッグやクリックの動作です。追加はマスクされる（除外される）範囲を広げ、削除はそれを戻してピクセルを保持します。Ctrl でもう一方になります。"),
    ZH_HANS("普通拖动或单击的作用。添加会扩大被遮罩（移除）的区域；减去会把它收回并保留这些像素。按住 Ctrl 则为另一种。"),
    ZH_HANT("一般拖曳或點一下的作用。新增會擴大被遮罩（移除）的區域；減去會把它收回並保留這些像素。按住 Ctrl 則為另一種。"),
    KO("일반 드래그나 클릭의 동작입니다. 추가는 마스크된(제거되는) 영역을 넓히고, 빼기는 그 영역을 되돌려 해당 픽셀을 유지합니다. Ctrl을 누르면 반대로 동작합니다."),
    DE("Was ein normales Ziehen oder Klicken tut. Hinzufügen vergrößert den maskierten (entfernten) Bereich; Abziehen nimmt ihn zurück und behält diese Pixel. Mit Ctrl das jeweils andere."),
    FR("Ce que fait un simple glisser ou clic. Ajouter agrandit la zone masquée (retirée) ; Soustraire la reprend et conserve ces pixels. Ctrl fait l'inverse."),
    ES("Lo que hace un arrastre o clic normal. Añadir amplía el área enmascarada (eliminada); Restar la recupera y conserva esos píxeles. Ctrl hace lo contrario."),
    PT("O que um arrastar ou clique normal faz. Adicionar aumenta a área mascarada (removida); Subtrair a recupera e mantém esses pixels. Ctrl faz o contrário."),
    IT("Cosa fa un normale trascinamento o clic. Aggiungi allarga l'area mascherata (rimossa); Sottrai la riprende e mantiene quei pixel. Ctrl fa l'opposto."),
    NL("Wat gewoon slepen of klikken doet. Toevoegen vergroot het gemaskeerde (verwijderde) gebied; Aftrekken neemt het terug en behoudt die pixels. Ctrl doet het andere."),
    RU("Что делает обычное перетаскивание или щелчок. «Добавить» расширяет замаскированную (удаляемую) область; «Вычесть» возвращает её и сохраняет эти пиксели. С Ctrl — наоборот."),
    TR("Normal sürükleme veya tıklamanın yaptığı şey. Ekle, maskelenen (kaldırılan) alanı büyütür; Çıkar onu geri alır ve o pikselleri tutar. Ctrl diğerini yapar."));

// ===========================================================================
// The status strip
// ===========================================================================

SS_MSG(hint_buttons,
    EN("Drag or Shift+drag: force drop. Ctrl+drag: force keep. Shift+Ctrl+drag: clear the correction. Right click closes a polygon."),
    JA("ドラッグまたは Shift+ドラッグ: 強制的に除外。Ctrl+ドラッグ: 強制的に保持。Shift+Ctrl+ドラッグ: 修正を消去。右クリックで多角形を閉じる。"),
    ZH_HANS("拖动或 Shift+拖动：强制丢弃。Ctrl+拖动：强制保留。Shift+Ctrl+拖动：清除修正。右键单击闭合多边形。"),
    ZH_HANT("拖曳或 Shift+拖曳：強制捨棄。Ctrl+拖曳：強制保留。Shift+Ctrl+拖曳：清除修正。右鍵點一下閉合多邊形。"),
    KO("드래그 또는 Shift+드래그: 강제 제외. Ctrl+드래그: 강제 유지. Shift+Ctrl+드래그: 수정 지우기. 오른쪽 클릭으로 다각형 닫기."),
    DE("Ziehen oder Shift+Ziehen: erzwungen verwerfen. Ctrl+Ziehen: erzwungen behalten. Shift+Ctrl+Ziehen: Korrektur löschen. Rechtsklick schließt ein Polygon."),
    FR("Glisser ou Shift+glisser : exclusion forcée. Ctrl+glisser : conservation forcée. Shift+Ctrl+glisser : effacer la correction. Un clic droit ferme un polygone."),
    ES("Arrastrar o Shift+arrastrar: descartar a la fuerza. Ctrl+arrastrar: conservar a la fuerza. Shift+Ctrl+arrastrar: borrar la corrección. Un clic derecho cierra un polígono."),
    PT("Arrastar ou Shift+arrastar: descartar à força. Ctrl+arrastar: manter à força. Shift+Ctrl+arrastar: apagar a correção. Um clique direito fecha um polígono."),
    IT("Trascina o Shift+trascina: scarta forzatamente. Ctrl+trascina: mantieni forzatamente. Shift+Ctrl+trascina: cancella la correzione. Un clic destro chiude un poligono."),
    NL("Slepen of Shift+slepen: geforceerd weglaten. Ctrl+slepen: geforceerd behouden. Shift+Ctrl+slepen: correctie wissen. Rechtsklik sluit een veelhoek."),
    RU("Перетаскивание или Shift+перетаскивание: принудительно убрать. Ctrl+перетаскивание: принудительно оставить. Shift+Ctrl+перетаскивание: стереть исправление. Правый щелчок замыкает многоугольник."),
    TR("Sürükleme veya Shift+sürükleme: zorla at. Ctrl+sürükleme: zorla tut. Shift+Ctrl+sürükleme: düzeltmeyi sil. Sağ tık çokgeni kapatır."));

SS_MSG(hint_view,
    EN("Wheel: zoom. Alt+wheel, [ and ]: brush or eraser size. Middle drag or Space+drag: pan. Esc: cancel the stroke."),
    JA("ホイール: ズーム。Alt+ホイール、[ と ]: ブラシまたは消しゴムのサイズ。中ボタンドラッグまたは Space+ドラッグ: 移動。Esc: ストロークを取り消し。"),
    ZH_HANS("滚轮：缩放。Alt+滚轮、[ 和 ]：画笔或橡皮擦大小。中键拖动或 Space+拖动：平移。Esc：取消笔画。"),
    ZH_HANT("滾輪：縮放。Alt+滾輪、[ 和 ]：筆刷或橡皮擦大小。中鍵拖曳或 Space+拖曳：平移。Esc：取消筆畫。"),
    KO("휠: 확대/축소. Alt+휠, [ 와 ]: 브러시 또는 지우개 크기. 가운데 버튼 드래그 또는 Space+드래그: 이동. Esc: 획 취소."),
    DE("Rad: Zoom. Alt+Rad, [ und ]: Pinsel- oder Radierergröße. Mittlere Taste ziehen oder Leertaste+Ziehen: verschieben. Esc: Strich abbrechen."),
    FR("Molette : zoom. Alt+molette, [ et ] : taille du pinceau ou de la gomme. Glisser avec le bouton du milieu ou Espace+glisser : déplacer. Échap : annuler le tracé."),
    ES("Rueda: zoom. Alt+rueda, [ y ]: tamaño del pincel o del borrador. Arrastrar con el botón central o Espacio+arrastrar: desplazar. Esc: cancelar el trazo."),
    PT("Roda: zoom. Alt+roda, [ e ]: tamanho do pincel ou da borracha. Arrastar com o botão do meio ou Espaço+arrastar: deslocar. Esc: cancelar o traço."),
    IT("Rotella: zoom. Alt+rotella, [ e ]: dimensione del pennello o della gomma. Trascina con il tasto centrale o Spazio+trascina: sposta. Esc: annulla il tratto."),
    NL("Wiel: zoomen. Alt+wiel, [ en ]: penseel- of gumgrootte. Slepen met middelste knop of spatie+slepen: verschuiven. Esc: streek annuleren."),
    RU("Колесо: масштаб. Alt+колесо, [ и ]: размер кисти или ластика. Перетаскивание средней кнопкой или Пробел+перетаскивание: сдвиг. Esc: отменить штрих."),
    TR("Tekerlek: yakınlaştırma. Alt+tekerlek, [ ve ]: fırça veya silgi boyutu. Orta tuşla veya Boşluk+sürükleme: kaydırma. Esc: çizimi iptal et."));

SS_MSG(hint_eraser,
    EN("Drag or Shift+drag: force keep. Ctrl+drag: force drop. Shift+Ctrl+drag: clear the correction."),
    JA("ドラッグまたは Shift+ドラッグ: 強制的に保持。Ctrl+ドラッグ: 強制的に除外。Shift+Ctrl+ドラッグ: 修正を消去。"),
    ZH_HANS("拖动或 Shift+拖动：强制保留。Ctrl+拖动：强制丢弃。Shift+Ctrl+拖动：清除修正。"),
    ZH_HANT("拖曳或 Shift+拖曳：強制保留。Ctrl+拖曳：強制捨棄。Shift+Ctrl+拖曳：清除修正。"),
    KO("드래그 또는 Shift+드래그: 강제 유지. Ctrl+드래그: 강제 제외. Shift+Ctrl+드래그: 수정 지우기."),
    DE("Ziehen oder Shift+Ziehen: erzwungen behalten. Ctrl+Ziehen: erzwungen verwerfen. Shift+Ctrl+Ziehen: Korrektur löschen."),
    FR("Glisser ou Shift+glisser : conservation forcée. Ctrl+glisser : exclusion forcée. Shift+Ctrl+glisser : effacer la correction."),
    ES("Arrastrar o Shift+arrastrar: conservar a la fuerza. Ctrl+arrastrar: descartar a la fuerza. Shift+Ctrl+arrastrar: borrar la corrección."),
    PT("Arrastar ou Shift+arrastar: manter à força. Ctrl+arrastar: descartar à força. Shift+Ctrl+arrastar: apagar a correção."),
    IT("Trascina o Shift+trascina: mantieni forzatamente. Ctrl+trascina: scarta forzatamente. Shift+Ctrl+trascina: cancella la correzione."),
    NL("Slepen of Shift+slepen: geforceerd behouden. Ctrl+slepen: geforceerd weglaten. Shift+Ctrl+slepen: correctie wissen."),
    RU("Перетаскивание или Shift+перетаскивание: принудительно оставить. Ctrl+перетаскивание: принудительно убрать. Shift+Ctrl+перетаскивание: стереть исправление."),
    TR("Sürükleme veya Shift+sürükleme: zorla tut. Ctrl+sürükleme: zorla at. Shift+Ctrl+sürükleme: düzeltmeyi sil."));

SS_MSG(status_frame,
    EN("Frame {0} of {1}: {2}"),
    JA("フレーム {0} / {1}: {2}"),
    ZH_HANS("第 {0} 帧，共 {1} 帧：{2}"),
    ZH_HANT("第 {0} 格，共 {1} 格：{2}"),
    KO("프레임 {0} / {1}: {2}"),
    DE("Bild {0} von {1}: {2}"),
    FR("Image {0} sur {1} : {2}"),
    ES("Fotograma {0} de {1}: {2}"),
    PT("Quadro {0} de {1}: {2}"),
    IT("Fotogramma {0} di {1}: {2}"),
    NL("Frame {0} van {1}: {2}"),
    RU("Кадр {0} из {1}: {2}"),
    TR("Kare {0} / {1}: {2}"));

SS_MSG(status_camera,
    EN("Camera: {0}"),
    JA("カメラ: {0}"),
    ZH_HANS("相机：{0}"),
    ZH_HANT("相機：{0}"),
    KO("카메라: {0}"),
    DE("Kamera: {0}"),
    FR("Caméra : {0}"),
    ES("Cámara: {0}"),
    PT("Câmera: {0}"),
    IT("Fotocamera: {0}"),
    NL("Camera: {0}"),
    RU("Камера: {0}"),
    TR("Kamera: {0}"));

SS_MSG(status_kept,
    EN("Kept: {0}%"),
    JA("保持: {0}%"),
    ZH_HANS("保留：{0}%"),
    ZH_HANT("保留：{0}%"),
    KO("유지: {0}%"),
    DE("Behalten: {0}%"),
    FR("Conservé : {0}%"),
    ES("Conservado: {0}%"),
    PT("Mantido: {0}%"),
    IT("Mantenuto: {0}%"),
    NL("Behouden: {0}%"),
    RU("Оставлено: {0}%"),
    TR("Tutulan: %{0}"));

SS_MSG(status_unsaved,
    EN("Unsaved changes"),
    JA("未保存の変更"),
    ZH_HANS("有未保存的更改"),
    ZH_HANT("有未儲存的變更"),
    KO("저장되지 않은 변경 사항"),
    DE("Ungespeicherte Änderungen"),
    FR("Modifications non enregistrées"),
    ES("Cambios sin guardar"),
    PT("Alterações não salvas"),
    IT("Modifiche non salvate"),
    NL("Niet-opgeslagen wijzigingen"),
    RU("Несохранённые изменения"),
    TR("Kaydedilmemiş değişiklikler"));

SS_MSG(status_saved,
    EN("Saved"),
    JA("保存済み"),
    ZH_HANS("已保存"),
    ZH_HANT("已儲存"),
    KO("저장됨"),
    DE("Gespeichert"),
    FR("Enregistré"),
    ES("Guardado"),
    PT("Salvo"),
    IT("Salvato"),
    NL("Opgeslagen"),
    RU("Сохранено"),
    TR("Kaydedildi"));

SS_MSG(status_base_regenerated,
    EN("A run regenerated this mask; the corrections were re-applied over the new one."),
    JA("実行によりこのマスクが再生成されたため、新しいマスクの上に修正を再適用しました。"),
    ZH_HANS("运行重新生成了此蒙版；修正已重新应用到新蒙版上。"),
    ZH_HANT("執行重新產生了此遮罩；修正已重新套用到新遮罩上。"),
    KO("실행이 이 마스크를 다시 생성했습니다. 수정은 새 마스크 위에 다시 적용되었습니다."),
    DE("Ein Lauf hat diese Maske neu erzeugt; die Korrekturen wurden auf die neue angewendet."),
    FR("Une exécution a régénéré ce masque ; les corrections ont été réappliquées sur le nouveau."),
    ES("Una ejecución regeneró esta máscara; las correcciones se volvieron a aplicar sobre la nueva."),
    PT("Uma execução regenerou esta máscara; as correções foram reaplicadas sobre a nova."),
    IT("Un'esecuzione ha rigenerato questa maschera; le correzioni sono state riapplicate sulla nuova."),
    NL("Een run heeft dit masker opnieuw gemaakt; de correcties zijn opnieuw toegepast op het nieuwe."),
    RU("Прогон заново создал эту маску; исправления применены поверх новой."),
    TR("Bir çalıştırma bu maskeyi yeniden oluşturdu; düzeltmeler yenisinin üzerine yeniden uygulandı."));

SS_MSG(status_base_missing,
    EN("No mask on disk for this frame. The corrections are kept until a run writes one."),
    JA("このフレームのマスクがディスクにありません。実行がマスクを書き出すまで修正は保持されます。"),
    ZH_HANS("磁盘上没有此帧的蒙版。修正将保留到某次运行写出蒙版为止。"),
    ZH_HANT("磁碟上沒有此影格的遮罩。修正會保留到某次執行寫出遮罩為止。"),
    KO("이 프레임의 마스크가 디스크에 없습니다. 실행이 마스크를 기록할 때까지 수정은 보관됩니다."),
    DE("Für dieses Bild liegt keine Maske auf der Platte. Die Korrekturen bleiben erhalten, bis ein Lauf eine schreibt."),
    FR("Aucun masque sur le disque pour cette image. Les corrections sont conservées jusqu'à ce qu'une exécution en écrive un."),
    ES("No hay máscara en disco para este fotograma. Las correcciones se conservan hasta que una ejecución escriba una."),
    PT("Não há máscara em disco para este quadro. As correções ficam guardadas até uma execução escrever uma."),
    IT("Nessuna maschera su disco per questo fotogramma. Le correzioni restano finché un'esecuzione non ne scrive una."),
    NL("Geen masker op schijf voor dit frame. De correcties blijven bewaard tot een run er een schrijft."),
    RU("На диске нет маски для этого кадра. Исправления сохраняются, пока прогон не запишет её."),
    TR("Bu kare için diskte maske yok. Bir çalıştırma maske yazana kadar düzeltmeler saklanır."));

SS_MSG(status_commit,
    EN("Last stroke: {0} ms"),
    JA("直前のストローク: {0} ms"),
    ZH_HANS("上一笔：{0} ms"),
    ZH_HANT("上一筆：{0} ms"),
    KO("마지막 획: {0} ms"),
    DE("Letzter Strich: {0} ms"),
    FR("Dernier tracé : {0} ms"),
    ES("Último trazo: {0} ms"),
    PT("Último traço: {0} ms"),
    IT("Ultimo tratto: {0} ms"),
    NL("Laatste streek: {0} ms"),
    RU("Последний штрих: {0} мс"),
    TR("Son çizim: {0} ms"));

SS_MSG(brush_radius,
    EN("Brush: {0} px"),
    JA("ブラシ: {0} px"),
    ZH_HANS("画笔：{0} px"),
    ZH_HANT("筆刷：{0} px"),
    KO("브러시: {0} px"),
    DE("Pinsel: {0} px"),
    FR("Pinceau : {0} px"),
    ES("Pincel: {0} px"),
    PT("Pincel: {0} px"),
    IT("Pennello: {0} px"),
    NL("Penseel: {0} px"),
    RU("Кисть: {0} px"),
    TR("Fırça: {0} px"));

SS_MSG(eraser_radius,
    EN("Eraser: {0} px"),
    JA("消しゴム: {0} px"),
    ZH_HANS("橡皮擦：{0} px"),
    ZH_HANT("橡皮擦：{0} px"),
    KO("지우개: {0} px"),
    DE("Radierer: {0} px"),
    FR("Gomme : {0} px"),
    ES("Borrador: {0} px"),
    PT("Borracha: {0} px"),
    IT("Gomma: {0} px"),
    NL("Gum: {0} px"),
    RU("Ластик: {0} px"),
    TR("Silgi: {0} px"));

SS_MSG(radius_keys,
    EN("[ ] Alt+wheel"),
    JA("[ ] Alt+ホイール"),
    ZH_HANS("[ ] Alt+滚轮"),
    ZH_HANT("[ ] Alt+滾輪"),
    KO("[ ] Alt+휠"),
    DE("[ ] Alt+Rad"),
    FR("[ ] Alt+molette"),
    ES("[ ] Alt+rueda"),
    PT("[ ] Alt+roda"),
    IT("[ ] Alt+rotella"),
    NL("[ ] Alt+wiel"),
    RU("[ ] Alt+колесо"),
    TR("[ ] Alt+tekerlek"));

SS_MSG(radius_help,
    EN("Brush and eraser size, in mask pixels. [ and ] step it; Alt+wheel over the canvas is continuous."),
    JA("ブラシと消しゴムのサイズ（マスクのピクセル単位）。[ と ] は段階的に、キャンバス上の Alt+ホイールは連続的に変えます。"),
    ZH_HANS("画笔和橡皮擦的大小，以蒙版像素计。[ 和 ] 逐级调整；在画布上 Alt+滚轮连续调整。"),
    ZH_HANT("筆刷和橡皮擦的大小，以遮罩像素計。[ 和 ] 逐級調整；在畫布上 Alt+滾輪連續調整。"),
    KO("브러시와 지우개의 크기(마스크 픽셀). [ 와 ] 는 단계적으로, 캔버스 위의 Alt+휠은 연속적으로 바꿉니다."),
    DE("Größe von Pinsel und Radierer, in Maskenpixeln. [ und ] ändern sie schrittweise, Alt+Rad über der Leinwand stufenlos."),
    FR("Taille du pinceau et de la gomme, en pixels de masque. [ et ] la changent par paliers ; Alt+molette sur le canevas, en continu."),
    ES("Tamaño del pincel y del borrador, en píxeles de máscara. [ y ] lo cambian por pasos; Alt+rueda sobre el lienzo, de forma continua."),
    PT("Tamanho do pincel e da borracha, em pixels da máscara. [ e ] alteram-no por passos; Alt+roda sobre a tela, de forma contínua."),
    IT("Dimensione del pennello e della gomma, in pixel della maschera. [ e ] la cambiano a passi; Alt+rotella sulla tela, in modo continuo."),
    NL("Grootte van penseel en gum, in maskerpixels. [ en ] wijzigen die stapsgewijs, Alt+wiel boven het canvas vloeiend."),
    RU("Размер кисти и ластика, в пикселях маски. [ и ] меняют его ступенчато, Alt+колесо над холстом — плавно."),
    TR("Fırça ve silgi boyutu, maske pikseli cinsinden. [ ve ] adım adım değiştirir; tuval üzerinde Alt+tekerlek sürekli değiştirir."));

SS_MSG(corrected_count,
    EN("Corrected frames: {0}"),
    JA("修正済みフレーム: {0}"),
    ZH_HANS("已修正的帧：{0}"),
    ZH_HANT("已修正的影格：{0}"),
    KO("수정된 프레임: {0}"),
    DE("Korrigierte Bilder: {0}"),
    FR("Images corrigées : {0}"),
    ES("Fotogramas corregidos: {0}"),
    PT("Quadros corrigidos: {0}"),
    IT("Fotogrammi corretti: {0}"),
    NL("Gecorrigeerde frames: {0}"),
    RU("Исправленных кадров: {0}"),
    TR("Düzeltilen kareler: {0}"));

// ===========================================================================
// Errors, and the two lines the dataset run logs
// ===========================================================================

SS_MSG(err_workspace_inside_images,
    EN("The dataset folder is inside the photo folder, so the corrections have nowhere safe to live. Choose another output folder."),
    JA("データセットフォルダーが写真フォルダーの中にあるため、修正を安全に保存できる場所がありません。別の出力フォルダーを選んでください。"),
    ZH_HANS("数据集文件夹位于照片文件夹内，修正没有安全的存放位置。请选择其他输出文件夹。"),
    ZH_HANT("資料集資料夾位於照片資料夾內，修正沒有安全的存放位置。請選擇其他輸出資料夾。"),
    KO("데이터셋 폴더가 사진 폴더 안에 있어 수정을 안전하게 둘 곳이 없습니다. 다른 출력 폴더를 선택하세요."),
    DE("Der Datensatzordner liegt im Fotoordner, die Korrekturen hätten also keinen sicheren Platz. Wählen Sie einen anderen Ausgabeordner."),
    FR("Le dossier du jeu de données est dans le dossier des photos, les corrections n'ont donc aucun endroit sûr. Choisissez un autre dossier de sortie."),
    ES("La carpeta del conjunto de datos está dentro de la carpeta de fotos, así que las correcciones no tienen un lugar seguro. Elige otra carpeta de salida."),
    PT("A pasta do conjunto de dados está dentro da pasta das fotos, por isso as correções não têm um lugar seguro. Escolha outra pasta de saída."),
    IT("La cartella del dataset è dentro la cartella delle foto, quindi le correzioni non hanno un posto sicuro. Scegli un'altra cartella di output."),
    NL("De datasetmap staat in de fotomap, dus de correcties hebben geen veilige plek. Kies een andere uitvoermap."),
    RU("Папка набора данных находится внутри папки с фотографиями, поэтому исправлениям негде безопасно храниться. Выберите другую папку вывода."),
    TR("Veri kümesi klasörü fotoğraf klasörünün içinde olduğundan düzeltmelerin güvenle duracağı bir yer yok. Başka bir çıktı klasörü seçin."));

SS_MSG(err_no_frames,
    EN("No images were found under {0}."),
    JA("{0} の下に画像が見つかりませんでした。"),
    ZH_HANS("在 {0} 下未找到图像。"),
    ZH_HANT("在 {0} 下找不到影像。"),
    KO("{0} 아래에서 이미지를 찾지 못했습니다."),
    DE("Unter {0} wurden keine Bilder gefunden."),
    FR("Aucune image trouvée sous {0}."),
    ES("No se encontraron imágenes en {0}."),
    PT("Não foram encontradas imagens em {0}."),
    IT("Nessuna immagine trovata in {0}."),
    NL("Geen afbeeldingen gevonden onder {0}."),
    RU("В {0} не найдено изображений."),
    TR("{0} altında görüntü bulunamadı."));

SS_MSG(err_read,
    EN("Could not read {0}."),
    JA("{0} を読み込めませんでした。"),
    ZH_HANS("无法读取 {0}。"),
    ZH_HANT("無法讀取 {0}。"),
    KO("{0}을(를) 읽을 수 없습니다."),
    DE("{0} konnte nicht gelesen werden."),
    FR("Impossible de lire {0}."),
    ES("No se pudo leer {0}."),
    PT("Não foi possível ler {0}."),
    IT("Impossibile leggere {0}."),
    NL("Kon {0} niet lezen."),
    RU("Не удалось прочитать {0}."),
    TR("{0} okunamadı."));

SS_MSG(err_write,
    EN("Could not write {0}."),
    JA("{0} を書き込めませんでした。"),
    ZH_HANS("无法写入 {0}。"),
    ZH_HANT("無法寫入 {0}。"),
    KO("{0}을(를) 쓸 수 없습니다."),
    DE("{0} konnte nicht geschrieben werden."),
    FR("Impossible d'écrire {0}."),
    ES("No se pudo escribir {0}."),
    PT("Não foi possível escrever {0}."),
    IT("Impossibile scrivere {0}."),
    NL("Kon {0} niet schrijven."),
    RU("Не удалось записать {0}."),
    TR("{0} yazılamadı."));

SS_MSG(err_other_mask_root,
    EN("The corrections in this project were made against the mask folder {0}, so they cannot be read against another one. Point the run back at that folder, or discard the corrections."),
    JA("このプロジェクトの修正はマスクフォルダー {0} に対して行われたため、別のフォルダーには適用できません。実行をそのフォルダーに戻すか、修正を破棄してください。"),
    ZH_HANS("本项目的修正是针对蒙版文件夹 {0} 做的，无法用于其他文件夹。请把运行指回该文件夹，或放弃这些修正。"),
    ZH_HANT("本專案的修正是針對遮罩資料夾 {0} 做的，無法用於其他資料夾。請把執行指回該資料夾，或捨棄這些修正。"),
    KO("이 프로젝트의 수정은 마스크 폴더 {0}을(를) 기준으로 이루어졌으므로 다른 폴더에는 적용할 수 없습니다. 실행을 그 폴더로 되돌리거나 수정을 버리세요."),
    DE("Die Korrekturen dieses Projekts entstanden gegen den Maskenordner {0} und lassen sich nicht auf einen anderen anwenden. Richten Sie den Lauf wieder auf diesen Ordner oder verwerfen Sie die Korrekturen."),
    FR("Les corrections de ce projet ont été faites sur le dossier de masques {0} et ne peuvent pas servir pour un autre. Repointez le traitement sur ce dossier, ou abandonnez les corrections."),
    ES("Las correcciones de este proyecto se hicieron sobre la carpeta de máscaras {0}, así que no sirven para otra. Vuelve a apuntar la ejecución a esa carpeta o descarta las correcciones."),
    PT("As correções deste projeto foram feitas sobre a pasta de máscaras {0}, por isso não servem para outra. Aponte a execução de volta para essa pasta ou descarte as correções."),
    IT("Le correzioni di questo progetto sono state fatte sulla cartella di maschere {0}, quindi non valgono per un'altra. Riporta l'esecuzione su quella cartella, oppure scarta le correzioni."),
    NL("De correcties in dit project zijn gemaakt op de maskermap {0} en gelden niet voor een andere. Richt de run weer op die map, of gooi de correcties weg."),
    RU("Исправления в этом проекте сделаны для папки масок {0}, поэтому к другой они неприменимы. Верните запуск к этой папке или откажитесь от исправлений."),
    TR("Bu projedeki düzeltmeler {0} maske klasörüne göre yapıldı, bu yüzden başka bir klasöre uygulanamaz. Çalışmayı o klasöre geri yönlendirin ya da düzeltmeleri atın."));

SS_MSG(err_other_mask_polarity,
    EN("The corrections in this project were made while the masks were read the other way round. Put \"Flip masks\" back as it was, or discard the corrections."),
    JA("このプロジェクトの修正は、マスクを逆の意味で読んでいたときに行われました。「マスクを反転」を元に戻すか、修正を破棄してください。"),
    ZH_HANS("本项目的修正是在以相反方式读取蒙版时做的。请把“反转蒙版”改回原样，或放弃这些修正。"),
    ZH_HANT("本專案的修正是在以相反方式讀取遮罩時做的。請把「反轉遮罩」改回原樣，或捨棄這些修正。"),
    KO("이 프로젝트의 수정은 마스크를 반대로 읽던 때에 이루어졌습니다. \"마스크 반전\"을 원래대로 되돌리거나 수정을 버리세요."),
    DE("Die Korrekturen dieses Projekts entstanden, als die Masken umgekehrt gelesen wurden. Stellen Sie \"Masken umkehren\" zurück oder verwerfen Sie die Korrekturen."),
    FR("Les corrections de ce projet ont été faites quand les masques étaient lus dans l'autre sens. Remettez « Inverser les masques » comme avant, ou abandonnez les corrections."),
    ES("Las correcciones de este proyecto se hicieron cuando las máscaras se leían al revés. Deja «Invertir las máscaras» como estaba o descarta las correcciones."),
    PT("As correções deste projeto foram feitas quando as máscaras eram lidas ao contrário. Reponha \"Inverter as máscaras\" como estava ou descarte as correções."),
    IT("Le correzioni di questo progetto sono state fatte quando le maschere erano lette al contrario. Rimetti \"Invertire le maschere\" com'era, oppure scarta le correzioni."),
    NL("De correcties in dit project zijn gemaakt toen de maskers andersom werden gelezen. Zet \"Maskers omkeren\" terug zoals het was, of gooi de correcties weg."),
    RU("Исправления в этом проекте сделаны, когда маски читались наоборот. Верните «Инвертировать маски» как было или откажитесь от исправлений."),
    TR("Bu projedeki düzeltmeler maskeler ters okunurken yapıldı. \"Maskeleri ters çevir\" ayarını eski hâline getirin ya da düzeltmeleri atın."));

SS_MSG(err_size_mismatch,
    EN("The correction layer for {0} is {1}x{2} but the mask is {3}x{4}; the layer was ignored."),
    JA("{0} の修正レイヤーは {1}x{2} ですが、マスクは {3}x{4} です。レイヤーは無視されました。"),
    ZH_HANS("{0} 的修正图层为 {1}x{2}，但蒙版为 {3}x{4}；该图层已被忽略。"),
    ZH_HANT("{0} 的修正圖層為 {1}x{2}，但遮罩為 {3}x{4}；該圖層已被忽略。"),
    KO("{0}의 수정 레이어는 {1}x{2}이지만 마스크는 {3}x{4}입니다. 레이어를 무시했습니다."),
    DE("Die Korrekturebene für {0} ist {1}x{2}, die Maske aber {3}x{4}; die Ebene wurde ignoriert."),
    FR("Le calque de correction de {0} fait {1}x{2} mais le masque fait {3}x{4} ; le calque a été ignoré."),
    ES("La capa de corrección de {0} es de {1}x{2} pero la máscara es de {3}x{4}; la capa se ignoró."),
    PT("A camada de correção de {0} tem {1}x{2} mas a máscara tem {3}x{4}; a camada foi ignorada."),
    IT("Il livello di correzione di {0} è {1}x{2} ma la maschera è {3}x{4}; il livello è stato ignorato."),
    NL("De correctielaag van {0} is {1}x{2} maar het masker is {3}x{4}; de laag is genegeerd."),
    RU("Слой исправлений для {0} имеет размер {1}x{2}, а маска {3}x{4}; слой пропущен."),
    TR("{0} için düzeltme katmanı {1}x{2} ama maske {3}x{4}; katman yok sayıldı."));

SS_MSG(log_recomposited,
    EN("Mask corrections re-applied over regenerated masks: {0}"),
    JA("再生成されたマスクへの修正の再適用: {0}"),
    ZH_HANS("已重新应用到重新生成蒙版上的修正：{0}"),
    ZH_HANT("已重新套用到重新產生遮罩上的修正：{0}"),
    KO("다시 생성된 마스크 위에 다시 적용한 수정: {0}"),
    DE("Auf neu erzeugte Masken wieder angewendete Korrekturen: {0}"),
    FR("Corrections réappliquées sur des masques régénérés : {0}"),
    ES("Correcciones reaplicadas sobre máscaras regeneradas: {0}"),
    PT("Correções reaplicadas sobre máscaras regeneradas: {0}"),
    IT("Correzioni riapplicate su maschere rigenerate: {0}"),
    NL("Correcties opnieuw toegepast op opnieuw gemaakte maskers: {0}"),
    RU("Исправлений применено поверх заново созданных масок: {0}"),
    TR("Yeniden oluşturulan maskelere yeniden uygulanan düzeltmeler: {0}"));

SS_MSG(log_recomposite_failed,
    EN("Could not re-apply the mask corrections: {0}"),
    JA("マスクの修正を再適用できませんでした: {0}"),
    ZH_HANS("无法重新应用蒙版修正：{0}"),
    ZH_HANT("無法重新套用遮罩修正：{0}"),
    KO("마스크 수정을 다시 적용할 수 없습니다: {0}"),
    DE("Die Maskenkorrekturen konnten nicht wieder angewendet werden: {0}"),
    FR("Impossible de réappliquer les corrections de masque : {0}"),
    ES("No se pudieron reaplicar las correcciones de máscara: {0}"),
    PT("Não foi possível reaplicar as correções de máscara: {0}"),
    IT("Impossibile riapplicare le correzioni delle maschere: {0}"),
    NL("Kon de maskercorrecties niet opnieuw toepassen: {0}"),
    RU("Не удалось заново применить исправления масок: {0}"),
    TR("Maske düzeltmeleri yeniden uygulanamadı: {0}"));

// ===========================================================================
// The pen tool (plan 2)
// ===========================================================================

SS_MSG(tool_path,
    EN("Path"),       JA("パス"),       ZH_HANS("路径"),  ZH_HANT("路徑"),
    KO("패스"),        DE("Pfad"),       FR("Chemin"),
    ES("Trazado"),    PT("Traçado"),    IT("Tracciato"),
    NL("Pad"),        RU("Контур"),     TR("Yol"));

SS_MSG(hint_path,
    EN("Click along an edge to drop anchors; the path snaps to the edge. Ctrl+click the "
       "first anchor to keep instead, Shift+Ctrl to clear. Click the first anchor, Enter "
       "or right-click closes it and paints its inside. Ctrl+Z takes an anchor back; Esc "
       "cancels."),
    JA("輪郭に沿ってクリックして点を置くと、パスが輪郭に沿います。最初の点を Ctrl+クリック"
       "すると保持になり、Shift+Ctrl でクリアします。最初の点をクリックするか Enter か右ク"
       "リックで閉じ、内側を塗ります。Ctrl+Z で点を戻し、Esc で中止します。"),
    ZH_HANS("沿边缘点击放下锚点，路径会贴合边缘。Ctrl+点击第一个锚点改为保留，Shift+Ctrl "
            "则清除。点击第一个锚点、按 Enter 或右键闭合并涂抹其内部。Ctrl+Z 撤回一个锚点，"
            "Esc 取消。"),
    ZH_HANT("沿邊緣點擊放下錨點，路徑會貼合邊緣。Ctrl+點擊第一個錨點改為保留，Shift+Ctrl "
            "則清除。點擊第一個錨點、按 Enter 或右鍵閉合並塗抹其內部。Ctrl+Z 收回一個錨點，"
            "Esc 取消。"),
    KO("윤곽을 따라 클릭해 앵커를 놓으면 경로가 윤곽에 붙습니다. 첫 앵커를 Ctrl+클릭하면 "
       "유지로 바뀌고, Shift+Ctrl은 지웁니다. 첫 앵커 클릭, Enter 또는 오른쪽 클릭으로 닫고 "
       "안쪽을 칠합니다. Ctrl+Z는 앵커를 되돌리고 Esc는 취소합니다."),
    DE("Entlang einer Kante klicken, um Anker zu setzen; der Pfad legt sich an die Kante. "
       "Strg+Klick auf den ersten Anker behält ihn stattdessen, Umschalt+Strg löscht die "
       "Korrektur. Erster Anker, Eingabe oder Rechtsklick schließt ihn und malt sein "
       "Inneres. Strg+Z nimmt einen Anker zurück, Esc bricht ab."),
    FR("Cliquez le long d'un contour pour poser des ancres ; le chemin épouse le contour. "
       "Ctrl+clic sur la première ancre la conserve à la place, Shift+Ctrl efface. La "
       "première ancre, Entrée ou un clic droit le ferme et peint son intérieur. Ctrl+Z "
       "retire une ancre, Échap annule."),
    ES("Haga clic a lo largo de un borde para poner anclas; el trazado se ajusta al borde. "
       "Ctrl+clic en la primera ancla la conserva en su lugar, Shift+Ctrl la borra. La "
       "primera ancla, Intro o clic derecho lo cierra y pinta su interior. Ctrl+Z quita "
       "un ancla; Esc cancela."),
    PT("Clique ao longo de um contorno para pôr âncoras; o traçado cola-se ao contorno. "
       "Ctrl+clique na primeira âncora mantém-na em vez disso, Shift+Ctrl apaga. A "
       "primeira âncora, Enter ou clique direito fecha-o e pinta o interior. Ctrl+Z retira "
       "uma âncora; Esc cancela."),
    IT("Fai clic lungo un bordo per posare ancoraggi; il tracciato segue il bordo. Ctrl+clic "
       "sul primo ancoraggio lo mantiene invece, Shift+Ctrl cancella. Il primo ancoraggio, "
       "Invio o clic destro lo chiude e ne dipinge l'interno. Ctrl+Z toglie un ancoraggio; "
       "Esc annulla."),
    NL("Klik langs een rand om ankers te zetten; het pad volgt de rand. Ctrl+klik op het "
       "eerste anker behoudt het juist, Shift+Ctrl wist. Het eerste anker, Enter of "
       "rechtsklik sluit het en schildert de binnenkant. Ctrl+Z neemt een anker terug; Esc "
       "breekt af."),
    RU("Щёлкайте вдоль края, чтобы ставить опорные точки; контур прилипает к краю. "
       "Ctrl+щелчок по первой точке сохраняет её, Shift+Ctrl стирает. Первая точка, Enter "
       "или правая кнопка замыкают его и закрашивают внутренность. Ctrl+Z убирает точку, "
       "Esc отменяет."),
    TR("Kenar boyunca tıklayarak çapalar bırakın; yol kenara yapışır. İlk çapaya "
       "Ctrl+tıklamak onu tutar, Shift+Ctrl siler. İlk çapa, Enter veya sağ tık onu kapatır "
       "ve içini boyar. Ctrl+Z bir çapayı geri alır; Esc iptal eder."));

SS_MSG(path_building,
    EN("Preparing the edge map for this frame..."),
    JA("このフレームの輪郭マップを準備中..."),
    ZH_HANS("正在为此帧准备边缘图..."),
    ZH_HANT("正在為此影格準備邊緣圖..."),
    KO("이 프레임의 윤곽 맵을 준비하는 중..."),
    DE("Kantenkarte für dieses Bild wird vorbereitet..."),
    FR("Préparation de la carte des contours de cette image..."),
    ES("Preparando el mapa de bordes de este fotograma..."),
    PT("Preparando o mapa de contornos deste quadro..."),
    IT("Preparazione della mappa dei bordi di questo fotogramma..."),
    NL("Randkaart voor dit frame wordt voorbereid..."),
    RU("Подготовка карты краёв для этого кадра..."),
    TR("Bu kare için kenar haritası hazırlanıyor..."));

SS_MSG(path_edge_map,
    EN("Edge map: {0}x{1}, step {2}, built in {3} ms"),
    JA("輪郭マップ: {0}x{1}、間隔 {2}、作成 {3} ms"),
    ZH_HANS("边缘图：{0}x{1}，步长 {2}，用时 {3} ms"),
    ZH_HANT("邊緣圖：{0}x{1}，步長 {2}，用時 {3} ms"),
    KO("윤곽 맵: {0}x{1}, 간격 {2}, 생성 {3} ms"),
    DE("Kantenkarte: {0}x{1}, Schritt {2}, erstellt in {3} ms"),
    FR("Carte des contours : {0}x{1}, pas {2}, calculée en {3} ms"),
    ES("Mapa de bordes: {0}x{1}, paso {2}, calculado en {3} ms"),
    PT("Mapa de contornos: {0}x{1}, passo {2}, calculado em {3} ms"),
    IT("Mappa dei bordi: {0}x{1}, passo {2}, calcolata in {3} ms"),
    NL("Randkaart: {0}x{1}, stap {2}, gemaakt in {3} ms"),
    RU("Карта краёв: {0}x{1}, шаг {2}, построена за {3} мс"),
    TR("Kenar haritası: {0}x{1}, adım {2}, {3} ms içinde oluşturuldu"));

SS_MSG(path_straight,
    EN("No edge map for this frame; the path uses straight segments."),
    JA("このフレームには輪郭マップがないため、パスは直線で結ばれます。"),
    ZH_HANS("此帧没有边缘图，路径使用直线段。"),
    ZH_HANT("此影格沒有邊緣圖，路徑使用直線段。"),
    KO("이 프레임에는 윤곽 맵이 없어 경로가 직선으로 이어집니다."),
    DE("Keine Kantenkarte für dieses Bild; der Pfad verwendet gerade Abschnitte."),
    FR("Pas de carte des contours pour cette image ; le chemin utilise des segments droits."),
    ES("No hay mapa de bordes para este fotograma; el trazado usa segmentos rectos."),
    PT("Sem mapa de contornos para este quadro; o traçado usa segmentos retos."),
    IT("Nessuna mappa dei bordi per questo fotogramma; il tracciato usa segmenti retti."),
    NL("Geen randkaart voor dit frame; het pad gebruikt rechte stukken."),
    RU("Для этого кадра нет карты краёв; контур строится прямыми отрезками."),
    TR("Bu kare için kenar haritası yok; yol düz parçalar kullanır."));

SS_MSG(path_anchors,
    EN("Path anchors: {0}"),
    JA("パスの点: {0}"),
    ZH_HANS("路径锚点：{0}"),
    ZH_HANT("路徑錨點：{0}"),
    KO("패스 앵커: {0}"),
    DE("Pfadanker: {0}"),
    FR("Ancres du chemin : {0}"),
    ES("Anclas del trazado: {0}"),
    PT("Âncoras do traçado: {0}"),
    IT("Ancoraggi del tracciato: {0}"),
    NL("Padankers: {0}"),
    RU("Точек контура: {0}"),
    TR("Yol çapaları: {0}"));

// ===========================================================================
// The pen (Bezier curves), with the names vector editors give the tool
// ===========================================================================

SS_MSG(tool_pen,
    EN("Pen"),        JA("ペン"),        ZH_HANS("钢笔"),  ZH_HANT("鋼筆"),
    KO("펜"),          DE("Zeichenstift"), FR("Plume"),
    ES("Pluma"),      PT("Caneta"),      IT("Penna"),
    NL("Pen"),        RU("Перо"),        TR("Kalem"));

SS_MSG(hint_pen,
    EN("Click for a corner, drag for a curve. Shift keeps 45-degree steps; Alt while "
       "dragging moves one handle alone; Space while dragging moves the anchor. Click the "
       "last anchor to straighten the next segment; Ctrl+drag moves a point or handle "
       "already placed. Hold Ctrl (keep) or Shift+Ctrl (clear) for the first anchor. Click "
       "the first anchor, Enter or right-click closes it and paints its inside. Ctrl+Z or "
       "Backspace takes an anchor back; Esc cancels."),
    JA("クリックで角、ドラッグで曲線の点を置きます。Shift で 45 度刻み、ドラッグ中の Alt で"
       "片方のハンドルだけを、ドラッグ中の Space で点そのものを動かします。最後の点を"
       "クリックすると次の線分がまっすぐになり、Ctrl+ドラッグで置いた点やハンドルを"
       "動かせます。最初の点を置くときに Ctrl で保持、Shift+Ctrl でクリアになります。"
       "最初の点のクリック、Enter か右クリックで閉じて内側を塗ります。Ctrl+Z か "
       "Backspace で点を戻し、Esc で中止します。"),
    ZH_HANS("点击放下角点，拖动放下曲线点。Shift 以 45 度为步长；拖动时按 Alt 只移动一侧"
            "手柄；拖动时按 Space 移动锚点本身。点击最后一个锚点可让下一段变直；Ctrl+拖动"
            "可移动已放下的点或手柄。放下第一个锚点时按住 Ctrl 为保留，Shift+Ctrl 为清除。"
            "点击第一个锚点、按 Enter 或右键闭合并涂抹其内部。Ctrl+Z 或 Backspace 撤回"
            "一个锚点，Esc 取消。"),
    ZH_HANT("點擊放下角點，拖曳放下曲線點。Shift 以 45 度為步長；拖曳時按 Alt 只移動一側"
            "控制把手；拖曳時按 Space 移動錨點本身。點擊最後一個錨點可讓下一段變直；"
            "Ctrl+拖曳可移動已放下的點或控制把手。放下第一個錨點時按住 Ctrl 為保留，"
            "Shift+Ctrl 為清除。點擊第一個錨點、按 Enter 或右鍵閉合並塗抹其內部。Ctrl+Z "
            "或 Backspace 收回一個錨點，Esc 取消。"),
    KO("클릭하면 모서리, 드래그하면 곡선 앵커를 놓습니다. Shift는 45도 단위로 맞추고, "
       "드래그 중 Alt는 핸들 한쪽만, 드래그 중 Space는 앵커 자체를 옮깁니다. 마지막 앵커를 "
       "클릭하면 다음 선분이 곧게 이어지고, Ctrl+드래그로 이미 놓은 점이나 핸들을 옮깁니다. "
       "첫 앵커를 놓을 때 Ctrl은 유지, Shift+Ctrl은 지우기입니다. 첫 앵커 클릭, Enter 또는 "
       "오른쪽 클릭으로 닫고 안쪽을 칠합니다. Ctrl+Z나 Backspace는 앵커를 되돌리고 Esc는 "
       "취소합니다."),
    DE("Klick setzt eine Ecke, Ziehen einen Kurvenpunkt. Umschalt rastet in 45-Grad-"
       "Schritten ein; Alt beim Ziehen bewegt nur einen Griff, Leertaste beim Ziehen den "
       "Anker selbst. Ein Klick auf den letzten Anker macht den nächsten Abschnitt gerade; "
       "Strg+Ziehen verschiebt einen schon gesetzten Punkt oder Griff. Strg (behalten) oder "
       "Umschalt+Strg (löschen) beim ersten Anker halten. Erster Anker, Eingabe oder "
       "Rechtsklick schließt ihn und malt sein Inneres. Strg+Z oder Rücktaste nimmt einen "
       "Anker zurück, Esc bricht ab."),
    FR("Un clic pose un angle, un glissé un point de courbe. Maj force des pas de 45 degrés ; "
       "Alt pendant le glissé déplace une seule poignée, Espace déplace l'ancre elle-même. "
       "Un clic sur la dernière ancre rend le segment suivant droit ; Ctrl+glisser déplace "
       "un point ou une poignée déjà posé. Maintenez Ctrl (conserver) ou Maj+Ctrl (effacer) "
       "pour la première ancre. La première ancre, Entrée ou un clic droit le ferme et peint "
       "son intérieur. Ctrl+Z ou Retour arrière retire une ancre, Échap annule."),
    ES("Un clic pone una esquina y un arrastre, un punto de curva. Mayús fija pasos de 45 "
       "grados; Alt al arrastrar mueve un solo tirador y Espacio al arrastrar mueve el "
       "ancla. Un clic en la última ancla deja recto el siguiente segmento; Ctrl+arrastrar "
       "mueve un punto o tirador ya puesto. Mantenga Ctrl (conservar) o Mayús+Ctrl (borrar) "
       "para la primera ancla. La primera ancla, Intro o clic derecho lo cierra y pinta su "
       "interior. Ctrl+Z o Retroceso quita un ancla; Esc cancela."),
    PT("Um clique põe um canto e um arraste, um ponto de curva. Shift fixa passos de 45 "
       "graus; Alt ao arrastar move só uma alça e Espaço ao arrastar move a própria âncora. "
       "Um clique na última âncora deixa reto o segmento seguinte; Ctrl+arrastar move um "
       "ponto ou alça já posto. Segure Ctrl (manter) ou Shift+Ctrl (limpar) na primeira "
       "âncora. A primeira âncora, Enter ou clique direito o fecha e pinta seu interior. "
       "Ctrl+Z ou Backspace tira uma âncora; Esc cancela."),
    IT("Un clic mette un angolo, un trascinamento un punto di curva. Maiusc blocca a passi "
       "di 45 gradi; Alt durante il trascinamento muove una sola maniglia, Spazio muove "
       "l'ancoraggio stesso. Un clic sull'ultimo ancoraggio rende dritto il segmento "
       "successivo; Ctrl+trascina sposta un punto o una maniglia già messi. Tenga premuto "
       "Ctrl (mantieni) o Maiusc+Ctrl (cancella) per il primo ancoraggio. Il primo "
       "ancoraggio, Invio o il clic destro lo chiude e ne dipinge l'interno. Ctrl+Z o "
       "Backspace toglie un ancoraggio; Esc annulla."),
    NL("Klik zet een hoek, slepen een kromme. Shift houdt stappen van 45 graden aan; Alt "
       "tijdens het slepen beweegt één greep, Spatie tijdens het slepen het anker zelf. Klik "
       "op het laatste anker om het volgende stuk recht te maken; Ctrl+slepen verplaatst een "
       "al gezet punt of greep. Houd Ctrl (behouden) of Shift+Ctrl (wissen) bij het eerste "
       "anker. Het eerste anker, Enter of rechtsklik sluit het en schildert de binnenkant. "
       "Ctrl+Z of Backspace neemt een anker terug; Esc breekt af."),
    RU("Щелчок ставит угол, перетаскивание — точку кривой. Shift держит шаг 45 градусов; "
       "Alt при перетаскивании двигает только одну ручку, пробел — саму точку. Щелчок по "
       "последней точке выпрямляет следующий отрезок; Ctrl+перетаскивание двигает уже "
       "поставленную точку или ручку. Держите Ctrl (сохранить) или Shift+Ctrl (стереть) для "
       "первой точки. Первая точка, Enter или правая кнопка замыкают контур и закрашивают "
       "внутренность. Ctrl+Z или Backspace убирает точку, Esc отменяет."),
    TR("Tıklama bir köşe, sürükleme bir eğri noktası bırakır. Shift 45 derecelik adımlar "
       "tutar; sürüklerken Alt tek bir tutamacı, Boşluk çapanın kendisini taşır. Son çapaya "
       "tıklamak sonraki parçayı düz yapar; Ctrl+sürükleme önceden konmuş bir noktayı ya da "
       "tutamacı taşır. İlk çapa için Ctrl (tut) veya Shift+Ctrl (temizle) basılı tutun. İlk "
       "çapa, Enter veya sağ tık onu kapatır ve içini boyar. Ctrl+Z veya Backspace bir "
       "çapayı geri alır; Esc iptal eder."));

SS_MSG(pen_anchors,
    EN("Pen anchors: {0}"),
    JA("ペンの点: {0}"),
    ZH_HANS("钢笔锚点：{0}"),
    ZH_HANT("鋼筆錨點：{0}"),
    KO("펜 앵커: {0}"),
    DE("Zeichenstift-Anker: {0}"),
    FR("Ancres de la plume : {0}"),
    ES("Anclas de la pluma: {0}"),
    PT("Âncoras da caneta: {0}"),
    IT("Ancoraggi della penna: {0}"),
    NL("Penankers: {0}"),
    RU("Точек пера: {0}"),
    TR("Kalem çapaları: {0}"));

// ===========================================================================
// SAM assist
// ===========================================================================

SS_MSG(sam_first_load,
    EN("Loading the checkpoint. The first prompt of a session takes a few seconds."),
    JA("チェックポイントを読み込み中です。セッション最初のプロンプトは数秒かかります。"),
    ZH_HANS("正在加载检查点。每次会话的第一个提示需要几秒钟。"),
    ZH_HANT("正在載入檢查點。每次工作階段的第一個提示需要幾秒鐘。"),
    KO("체크포인트를 불러오는 중입니다. 세션의 첫 프롬프트는 몇 초 걸립니다."),
    DE("Checkpoint wird geladen. Der erste Prompt einer Sitzung dauert einige Sekunden."),
    FR("Chargement du checkpoint. La première requête d'une session prend quelques secondes."),
    ES("Cargando el checkpoint. La primera indicación de una sesión tarda unos segundos."),
    PT("Carregando o checkpoint. O primeiro prompt de uma sessão leva alguns segundos."),
    IT("Caricamento del checkpoint. Il primo prompt di una sessione richiede alcuni secondi."),
    NL("Checkpoint wordt geladen. De eerste prompt van een sessie duurt een paar seconden."),
    RU("Загрузка контрольной точки. Первый запрос за сеанс занимает несколько секунд."),
    TR("Kontrol noktası yükleniyor. Bir oturumun ilk istemi birkaç saniye sürer."));

SS_MSG(sam_working,
    EN("Segmenting..."),
    JA("セグメント化中…"),
    ZH_HANS("正在分割…"),
    ZH_HANT("正在分割…"),
    KO("분할하는 중…"),
    DE("Wird segmentiert …"),
    FR("Segmentation…"),
    ES("Segmentando…"),
    PT("Segmentando…"),
    IT("Segmentazione…"),
    NL("Bezig met segmenteren…"),
    RU("Сегментация…"),
    TR("Bölütleniyor…"));

SS_MSG(sam_unavailable_build,
    EN("This build has no segmentation module, so SAM assist is unavailable."),
    JA("このビルドには分割モジュールがないため、SAM アシストは使えません。"),
    ZH_HANS("此版本不含分割模块，无法使用 SAM 辅助。"),
    ZH_HANT("此版本不含分割模組，無法使用 SAM 輔助。"),
    KO("이 빌드에는 분할 모듈이 없어 SAM 보조를 사용할 수 없습니다."),
    DE("Dieser Build enthält kein Segmentierungsmodul; die SAM-Hilfe ist nicht verfügbar."),
    FR("Cette version n'inclut pas le module de segmentation ; l'assistance SAM est indisponible."),
    ES("Esta compilación no incluye el módulo de segmentación; la ayuda de SAM no está disponible."),
    PT("Esta versão não inclui o módulo de segmentação; a assistência SAM não está disponível."),
    IT("Questa build non include il modulo di segmentazione; l'assistenza SAM non è disponibile."),
    NL("Deze build bevat geen segmentatiemodule; SAM-hulp is niet beschikbaar."),
    RU("В этой сборке нет модуля сегментации, поэтому помощь SAM недоступна."),
    TR("Bu sürümde bölütleme modülü yok; SAM yardımı kullanılamıyor."));

SS_MSG(sam_blocked_preview,
    EN("SAM assist is paused while the mask or depth preview is open. Close it to prompt here."),
    JA("マスクまたは深度のプレビューが開いている間、SAM アシストは停止します。ここで使うにはプレビューを閉じてください。"),
    ZH_HANS("蒙版或深度预览打开时，SAM 辅助暂停。关闭预览后即可在此使用。"),
    ZH_HANT("遮罩或深度預覽開啟時，SAM 輔助暫停。關閉預覽後即可在此使用。"),
    KO("마스크 또는 깊이 미리보기가 열려 있는 동안 SAM 보조가 일시 중지됩니다. 여기서 사용하려면 미리보기를 닫으세요."),
    DE("Die SAM-Hilfe pausiert, solange die Masken- oder Tiefenvorschau geöffnet ist. Schließen Sie sie, um hier zu arbeiten."),
    FR("L'assistance SAM est en pause tant que l'aperçu des masques ou de la profondeur est ouvert. Fermez-le pour l'utiliser ici."),
    ES("La ayuda de SAM se pausa mientras la vista previa de máscaras o de profundidad está abierta. Ciérrela para usarla aquí."),
    PT("A assistência SAM fica pausada enquanto a prévia de máscaras ou de profundidade está aberta. Feche-a para usá-la aqui."),
    IT("L'assistenza SAM è in pausa mentre l'anteprima delle maschere o della profondità è aperta. Chiudila per usarla qui."),
    NL("SAM-hulp is gepauzeerd zolang het masker- of dieptevoorbeeld open is. Sluit het om hier te werken."),
    RU("Помощь SAM приостановлена, пока открыт предпросмотр масок или глубины. Закройте его, чтобы работать здесь."),
    TR("Maske veya derinlik önizlemesi açıkken SAM yardımı duraklatılır. Burada kullanmak için önizlemeyi kapatın."));

SS_MSG(sam_blocked_run,
    EN("SAM assist is paused while a run is using the GPU."),
    JA("実行中の処理が GPU を使っている間、SAM アシストは停止します。"),
    ZH_HANS("有任务正在使用 GPU，SAM 辅助暂停。"),
    ZH_HANT("有工作正在使用 GPU，SAM 輔助暫停。"),
    KO("작업이 GPU를 사용하는 동안 SAM 보조가 일시 중지됩니다."),
    DE("Die SAM-Hilfe pausiert, solange ein Lauf die GPU nutzt."),
    FR("L'assistance SAM est en pause tant qu'une tâche utilise le GPU."),
    ES("La ayuda de SAM se pausa mientras una tarea usa la GPU."),
    PT("A assistência SAM fica pausada enquanto uma tarefa usa a GPU."),
    IT("L'assistenza SAM è in pausa mentre un'elaborazione usa la GPU."),
    NL("SAM-hulp is gepauzeerd zolang een taak de GPU gebruikt."),
    RU("Помощь SAM приостановлена, пока задача использует GPU."),
    TR("Bir iş GPU'yu kullanırken SAM yardımı duraklatılır."));

SS_MSG(tool_sam,
    EN("SAM"),        JA("SAM"),        ZH_HANS("SAM"),   ZH_HANT("SAM"),
    KO("SAM"),        DE("SAM"),        FR("SAM"),
    ES("SAM"),        PT("SAM"),        IT("SAM"),
    NL("SAM"),        RU("SAM"),        TR("SAM"));

SS_MSG(sam_hint,
    EN("Click an object to drop it; Ctrl+click keeps it; Shift+Ctrl+click clears its corrections; "
       "right-click a part to leave it out. Ctrl+Z takes back the last click. Esc cancels."),
    JA("オブジェクトをクリックすると除外します。Ctrl+クリックで保持、Shift+Ctrl+クリックで修正を消去します。"
       "右クリックでその部分を対象から外します。Ctrl+Z で最後のクリックを取り消します。Esc で中止します。"),
    ZH_HANS("点击物体将其丢弃；Ctrl+点击保留；Shift+Ctrl+点击清除其修正；右键点击某一部分可将其排除。"
            "Ctrl+Z 撤回最后一次点击。Esc 取消。"),
    ZH_HANT("點擊物件將其捨棄；Ctrl+點擊保留；Shift+Ctrl+點擊清除其修正；在某一部分按右鍵可將其排除。"
            "Ctrl+Z 收回最後一次點擊。Esc 取消。"),
    KO("개체를 클릭하면 제외합니다. Ctrl+클릭은 유지, Shift+Ctrl+클릭은 수정을 지웁니다. 일부를 오른쪽 "
       "클릭하면 대상에서 뺍니다. Ctrl+Z는 마지막 클릭을 되돌립니다. Esc로 취소합니다."),
    DE("Klicken Sie auf ein Objekt, um es zu verwerfen; Ctrl+Klick behält es; Shift+Ctrl+Klick löscht "
       "seine Korrekturen; ein Rechtsklick auf einen Teil nimmt ihn aus. Strg+Z nimmt den letzten "
       "Klick zurück. Esc bricht ab."),
    FR("Cliquez sur un objet pour l'exclure ; Ctrl+clic le conserve ; Shift+Ctrl+clic efface ses "
       "corrections ; un clic droit sur une partie l'en retire. Ctrl+Z retire le dernier clic. Esc annule."),
    ES("Haga clic en un objeto para descartarlo; Ctrl+clic lo conserva; Shift+Ctrl+clic borra sus "
       "correcciones; un clic derecho en una parte la excluye. Ctrl+Z quita el último clic. Esc cancela."),
    PT("Clique em um objeto para descartá-lo; Ctrl+clique o mantém; Shift+Ctrl+clique apaga suas "
       "correções; um clique direito em uma parte a exclui. Ctrl+Z desfaz o último clique. Esc cancela."),
    IT("Fai clic su un oggetto per scartarlo; Ctrl+clic lo mantiene; Shift+Ctrl+clic ne cancella le "
       "correzioni; il clic destro su una parte la esclude. Ctrl+Z toglie l'ultimo clic. Esc annulla."),
    NL("Klik op een object om het weg te laten; Ctrl+klik behoudt het; Shift+Ctrl+klik wist de correcties "
       "ervan; rechtsklik op een deel sluit het uit. Ctrl+Z neemt de laatste klik terug. Esc annuleert."),
    RU("Щёлкните объект, чтобы убрать его; Ctrl+щелчок оставляет его; Shift+Ctrl+щелчок стирает его "
       "исправления; правый щелчок по части исключает её. Ctrl+Z отменяет последний щелчок. Esc отменяет."),
    TR("Atmak için bir nesneye tıklayın; Ctrl+tık onu tutar; Shift+Ctrl+tık düzeltmelerini siler; bir "
       "parçaya sağ tık onu hariç tutar. Ctrl+Z son tıklamayı geri alır. Esc iptal eder."));

SS_MSG(sam_cancel_slow,
    EN("Esc cancels once the step already running finishes: loading the model, reading the frame, or finding the object."),
    JA("Esc で中止しますが、実行中の手順（モデルの読み込み、フレームの読み取り、オブジェクトの検出）が終わってからです。"),
    ZH_HANS("Esc 会在当前步骤（加载模型、读取画面或查找物体）完成后取消。"),
    ZH_HANT("Esc 會在目前步驟（載入模型、讀取畫面或尋找物件）完成後取消。"),
    KO("Esc는 진행 중인 단계(모델 불러오기, 프레임 읽기, 개체 찾기)가 끝난 뒤에 취소합니다."),
    DE("Esc bricht ab, sobald der laufende Schritt fertig ist: das Modell laden, das Bild lesen oder das Objekt finden."),
    FR("Esc annule une fois l'étape en cours terminée : chargement du modèle, lecture de l'image ou recherche de l'objet."),
    ES("Esc cancela cuando termina el paso en curso: cargar el modelo, leer el fotograma o buscar el objeto."),
    PT("Esc cancela quando a etapa em andamento termina: carregar o modelo, ler o quadro ou encontrar o objeto."),
    IT("Esc annulla al termine del passo in corso: caricamento del modello, lettura del fotogramma o ricerca dell'oggetto."),
    NL("Esc annuleert zodra de lopende stap klaar is: het model laden, het beeld lezen of het object zoeken."),
    RU("Esc отменяет после завершения текущего шага: загрузки модели, чтения кадра или поиска объекта."),
    TR("Esc, süren adım bitince iptal eder: modeli yükleme, kareyi okuma veya nesneyi bulma."));

SS_MSG(sam_result,
    EN("Added {0} px, detections: {1}, score {2}, {3} ms"),
    JA("追加 {0} px、検出数: {1}、スコア {2}、{3} ms"),
    ZH_HANS("已添加 {0} px，检测数：{1}，得分 {2}，{3} ms"),
    ZH_HANT("已新增 {0} px，偵測數：{1}，分數 {2}，{3} ms"),
    KO("추가 {0} px, 검출 수: {1}, 점수 {2}, {3} ms"),
    DE("Hinzugefügt: {0} px, Erkennungen: {1}, Score {2}, {3} ms"),
    FR("Ajouté : {0} px, détections : {1}, score {2}, {3} ms"),
    ES("Añadido: {0} px, detecciones: {1}, puntuación {2}, {3} ms"),
    PT("Adicionado: {0} px, detecções: {1}, pontuação {2}, {3} ms"),
    IT("Aggiunti: {0} px, rilevamenti: {1}, punteggio {2}, {3} ms"),
    NL("Toegevoegd: {0} px, detecties: {1}, score {2}, {3} ms"),
    RU("Добавлено: {0} пкс, обнаружений: {1}, оценка {2}, {3} мс"),
    TR("Eklenen: {0} px, algılama: {1}, puan {2}, {3} ms"));

SS_MSG(sam_text_label,
    EN("Text prompt"),
    JA("テキストプロンプト"),
    ZH_HANS("文本提示"),
    ZH_HANT("文字提示"),
    KO("텍스트 프롬프트"),
    DE("Text-Prompt"),
    FR("Requête texte"),
    ES("Indicación de texto"),
    PT("Prompt de texto"),
    IT("Prompt testuale"),
    NL("Tekstprompt"),
    RU("Текстовый запрос"),
    TR("Metin istemi"));

SS_MSG(sam_text_find,
    EN("Find"),       JA("検索"),       ZH_HANS("查找"),  ZH_HANT("尋找"),
    KO("찾기"),       DE("Suchen"),     FR("Chercher"),
    ES("Buscar"),     PT("Buscar"),     IT("Cerca"),
    NL("Zoeken"),     RU("Найти"),      TR("Bul"));

SS_MSG(sam_text_empty,
    EN("Type what to drop first."),
    JA("先に取り除くものを入力してください。"),
    ZH_HANS("请先输入要剔除的内容。"),
    ZH_HANT("請先輸入要剔除的內容。"),
    KO("먼저 제거할 대상을 입력하세요."),
    DE("Geben Sie zuerst ein, was entfernt werden soll."),
    FR("Saisissez d'abord ce qu'il faut retirer."),
    ES("Escriba primero qué quitar."),
    PT("Digite primeiro o que remover."),
    IT("Scrivi prima cosa rimuovere."),
    NL("Typ eerst wat er weg moet."),
    RU("Сначала введите, что убрать."),
    TR("Önce neyin kaldırılacağını yazın."));

SS_MSG(sam_text_unsupported,
    EN("Text prompts need SAM 3. Switch the model to SAM 3, or use clicks."),
    JA("テキストプロンプトには SAM 3 が必要です。モデルを SAM 3 に切り替えるか、クリックを使ってください。"),
    ZH_HANS("文本提示需要 SAM 3。请将模型切换为 SAM 3，或改用点击。"),
    ZH_HANT("文字提示需要 SAM 3。請將模型切換為 SAM 3，或改用點擊。"),
    KO("텍스트 프롬프트에는 SAM 3이 필요합니다. 모델을 SAM 3으로 전환하거나 클릭을 사용하세요."),
    DE("Textprompts benötigen SAM 3. Wechseln Sie das Modell zu SAM 3 oder verwenden Sie Klicks."),
    FR("Les prompts textuels nécessitent SAM 3. Passez le modèle à SAM 3, ou utilisez des clics."),
    ES("Los prompts de texto requieren SAM 3. Cambie el modelo a SAM 3 o use clics."),
    PT("Prompts de texto exigem o SAM 3. Mude o modelo para SAM 3 ou use cliques."),
    IT("I prompt testuali richiedono SAM 3. Passa il modello a SAM 3 oppure usa i clic."),
    NL("Tekstprompts vereisen SAM 3. Schakel het model over naar SAM 3 of gebruik klikken."),
    RU("Для текстовых подсказок нужна SAM 3. Переключите модель на SAM 3 или используйте щелчки."),
    TR("Metin istemleri SAM 3 gerektirir. Modeli SAM 3'e geçirin veya tıklamaları kullanın."));

SS_MSG(sam_empty,
    EN("The prompt matched nothing on this frame."),
    JA("このフレームではプロンプトに一致するものがありませんでした。"),
    ZH_HANS("该提示在此帧中没有匹配到任何内容。"),
    ZH_HANT("該提示在此影格中沒有符合任何內容。"),
    KO("이 프레임에서 프롬프트와 일치하는 것이 없습니다."),
    DE("Der Prompt hat in diesem Bild nichts gefunden."),
    FR("La requête ne correspond à rien sur cette image."),
    ES("La indicación no coincidió con nada en este fotograma."),
    PT("O prompt não encontrou nada neste quadro."),
    IT("Il prompt non ha trovato nulla in questo fotogramma."),
    NL("De prompt vond niets in dit frame."),
    RU("Запрос ничего не нашёл в этом кадре."),
    TR("İstem bu karede hiçbir şeyle eşleşmedi."));

SS_MSG(sam_vetoed_all,
    EN("Every match is covered by an exception, so nothing was dropped."),
    JA("一致したものはすべて例外に含まれるため、何も取り除きませんでした。"),
    ZH_HANS("所有匹配都属于例外，因此没有剔除任何内容。"),
    ZH_HANT("所有符合項目都屬於例外，因此沒有剔除任何內容。"),
    KO("일치한 것이 모두 예외에 해당하므로 아무것도 제거하지 않았습니다."),
    DE("Jeder Treffer fällt unter eine Ausnahme, daher wurde nichts entfernt."),
    FR("Chaque correspondance relève d'une exception, donc rien n'a été retiré."),
    ES("Cada coincidencia está cubierta por una excepción, así que no se quitó nada."),
    PT("Cada correspondência está coberta por uma exceção, então nada foi removido."),
    IT("Ogni corrispondenza rientra in un'eccezione, quindi non è stato rimosso nulla."),
    NL("Elke overeenkomst valt onder een uitzondering, dus er is niets verwijderd."),
    RU("Каждое совпадение попадает под исключение, поэтому ничего не убрано."),
    TR("Her eşleşme bir istisnaya giriyor, bu yüzden hiçbir şey kaldırılmadı."));

// ===========================================================================
// View modes and the peek
// ===========================================================================

SS_MSG(view_overlay,
    EN("Overlay"),
    JA("オーバーレイ"),
    ZH_HANS("叠加"),
    ZH_HANT("疊加"),
    KO("오버레이"),
    DE("Überlagerung"),
    FR("Superposition"),
    ES("Superposición"),
    PT("Sobreposição"),
    IT("Sovrapposizione"),
    NL("Overlay"),
    RU("Наложение"),
    TR("Bindirme"));

SS_MSG(view_mask_only,
    EN("Mask only"),
    JA("マスクのみ"),
    ZH_HANS("仅蒙版"),
    ZH_HANT("僅遮罩"),
    KO("마스크만"),
    DE("Nur Maske"),
    FR("Masque seul"),
    ES("Solo máscara"),
    PT("Só máscara"),
    IT("Solo maschera"),
    NL("Alleen masker"),
    RU("Только маска"),
    TR("Yalnızca maske"));

SS_MSG(view_side_by_side,
    EN("Side by side"),
    JA("並べて表示"),
    ZH_HANS("并排"),
    ZH_HANT("並排"),
    KO("나란히"),
    DE("Nebeneinander"),
    FR("Côte à côte"),
    ES("Lado a lado"),
    PT("Lado a lado"),
    IT("Affiancate"),
    NL("Naast elkaar"),
    RU("Рядом"),
    TR("Yan yana"));

SS_MSG(view_help,
    EN("Overlay tints the dropped pixels over the photo. Mask only shows the mask with the corrections tinted. Side by side shows the bare photo on the left and the mask on the right; both take strokes. V cycles the views."),
    JA("オーバーレイは写真の上で除外ピクセルに色を付けます。マスクのみは修正に色を付けたマスクを表示します。並べて表示は左に元の写真、右にマスクを表示し、どちらにも描き込めます。V で表示を切り替えます。"),
    ZH_HANS("叠加在照片上为丢弃的像素着色。仅蒙版显示蒙版并为修正着色。并排在左侧显示原始照片、右侧显示蒙版，两侧都可以绘制。按 V 切换视图。"),
    ZH_HANT("疊加在照片上為捨棄的像素著色。僅遮罩顯示遮罩並為修正著色。並排在左側顯示原始照片、右側顯示遮罩，兩側都可以繪製。按 V 切換檢視。"),
    KO("오버레이는 사진 위에 제외된 픽셀을 색으로 표시합니다. 마스크만은 수정을 색으로 표시한 마스크를 보여줍니다. 나란히는 왼쪽에 원본 사진, 오른쪽에 마스크를 보여주며 양쪽 모두에 그릴 수 있습니다. V 키로 보기를 전환합니다."),
    DE("Überlagerung färbt die verworfenen Pixel über dem Foto ein. Nur Maske zeigt die Maske mit eingefärbten Korrekturen. Nebeneinander zeigt links das reine Foto und rechts die Maske; beide nehmen Striche an. V wechselt die Ansicht."),
    FR("Superposition teinte les pixels exclus sur la photo. Masque seul montre le masque avec les corrections teintées. Côte à côte montre la photo nue à gauche et le masque à droite ; les deux acceptent les traits. V fait défiler les vues."),
    ES("Superposición tiñe los píxeles descartados sobre la foto. Solo máscara muestra la máscara con las correcciones teñidas. Lado a lado muestra la foto sin más a la izquierda y la máscara a la derecha; ambas aceptan trazos. V alterna las vistas."),
    PT("Sobreposição tinge os pixels descartados sobre a foto. Só máscara mostra a máscara com as correções tingidas. Lado a lado mostra a foto pura à esquerda e a máscara à direita; ambas aceitam traços. V alterna as vistas."),
    IT("Sovrapposizione colora i pixel scartati sopra la foto. Solo maschera mostra la maschera con le correzioni colorate. Affiancate mostra la foto nuda a sinistra e la maschera a destra; entrambe accettano i tratti. V scorre le viste."),
    NL("Overlay kleurt de weggelaten pixels over de foto. Alleen masker toont het masker met de correcties gekleurd. Naast elkaar toont links de kale foto en rechts het masker; beide nemen streken aan. V wisselt de weergave."),
    RU("Наложение подкрашивает убранные пиксели поверх фото. Только маска показывает маску с подкрашенными исправлениями. Рядом показывает слева чистое фото, справа маску; рисовать можно на обеих. V переключает виды."),
    TR("Bindirme, atılan pikselleri fotoğrafın üzerinde renklendirir. Yalnızca maske, düzeltmeleri renklendirilmiş maskeyi gösterir. Yan yana solda çıplak fotoğrafı, sağda maskeyi gösterir; ikisine de çizilebilir. V görünümleri değiştirir."));

SS_MSG(view_locked,
    EN("Finish the shape, or press Esc to cancel it, before changing the view."),
    JA("表示を切り替える前に、図形を完成させるか Esc で取り消してください。"),
    ZH_HANS("切换视图前，请先完成图形，或按 Esc 取消。"),
    ZH_HANT("切換檢視前，請先完成圖形，或按 Esc 取消。"),
    KO("보기를 바꾸기 전에 도형을 완성하거나 Esc 키로 취소하세요."),
    DE("Die Form fertigstellen oder mit Esc abbrechen, bevor die Ansicht gewechselt wird."),
    FR("Terminez la forme, ou appuyez sur Esc pour l'annuler, avant de changer de vue."),
    ES("Termina la forma, o pulsa Esc para cancelarla, antes de cambiar la vista."),
    PT("Termine a forma, ou pressione Esc para cancelá-la, antes de mudar a vista."),
    IT("Completa la forma, o premi Esc per annullarla, prima di cambiare vista."),
    NL("Maak de vorm af, of druk op Esc om hem te annuleren, voordat je de weergave wisselt."),
    RU("Закончите фигуру или нажмите Esc, чтобы отменить её, прежде чем менять вид."),
    TR("Görünümü değiştirmeden önce şekli tamamlayın ya da Esc ile iptal edin."));

SS_MSG(nav_locked,
    EN("Finish the shape, or press Esc to cancel it, before changing frames."),
    JA("フレームを切り替える前に、図形を完成させるか Esc で取り消してください。"),
    ZH_HANS("切换帧之前，请先完成图形，或按 Esc 取消。"),
    ZH_HANT("切換影格之前，請先完成圖形，或按 Esc 取消。"),
    KO("프레임을 바꾸기 전에 도형을 완성하거나 Esc 키로 취소하세요."),
    DE("Die Form fertigstellen oder mit Esc abbrechen, bevor das Bild gewechselt wird."),
    FR("Terminez la forme, ou appuyez sur Esc pour l'annuler, avant de changer d'image."),
    ES("Termina la forma, o pulsa Esc para cancelarla, antes de cambiar de fotograma."),
    PT("Termine a forma, ou pressione Esc para cancelá-la, antes de mudar de quadro."),
    IT("Completa la forma, o premi Esc per annullarla, prima di cambiare fotogramma."),
    NL("Maak de vorm af, of druk op Esc om hem te annuleren, voordat je van frame wisselt."),
    RU("Закончите фигуру или нажмите Esc, чтобы отменить её, прежде чем переходить к другому кадру."),
    TR("Kareyi değiştirmeden önce şekli tamamlayın ya da Esc ile iptal edin."));

SS_MSG(peek_hint,
    EN("Hold {0} over the picture to see the bare photo, Shift+{0} the bare mask."),
    JA("画像の上で {0} を押し続けると元の写真、Shift+{0} で元のマスクが見えます。"),
    ZH_HANS("在图片上按住 {0} 查看原始照片，按住 Shift+{0} 查看原始蒙版。"),
    ZH_HANT("在圖片上按住 {0} 檢視原始照片，按住 Shift+{0} 檢視原始遮罩。"),
    KO("그림 위에서 {0} 키를 누르고 있으면 원본 사진이, Shift+{0}이면 원본 마스크가 보입니다."),
    DE("{0} über dem Bild gedrückt halten zeigt das reine Foto, Shift+{0} die reine Maske."),
    FR("Maintenir {0} sur l'image montre la photo nue, Shift+{0} le masque nu."),
    ES("Mantén {0} sobre la imagen para ver la foto sin más, Shift+{0} la máscara sola."),
    PT("Mantenha {0} sobre a imagem para ver a foto pura, Shift+{0} a máscara pura."),
    IT("Tieni premuto {0} sull'immagine per vedere la foto nuda, Shift+{0} la maschera nuda."),
    NL("Houd {0} boven de afbeelding ingedrukt voor de kale foto, Shift+{0} voor het kale masker."),
    RU("Удерживайте {0} над изображением, чтобы увидеть чистое фото, Shift+{0} для чистой маски."),
    TR("Resmin üzerinde {0} tuşunu basılı tutun: çıplak fotoğraf; Shift+{0}: çıplak maske."));

SS_MSG(peek_hint_side,
    EN("Side by side: hold {0} to see the overlay in the mask pane, Shift+{0} the overlay in the photo pane."),
    JA("並べて表示では、{0} を押し続けるとマスク側にオーバーレイ、Shift+{0} で写真側にオーバーレイが出ます。"),
    ZH_HANS("并排时：按住 {0} 在蒙版窗格显示叠加，按住 Shift+{0} 在照片窗格显示叠加。"),
    ZH_HANT("並排時：按住 {0} 在遮罩窗格顯示疊加，按住 Shift+{0} 在照片窗格顯示疊加。"),
    KO("나란히 보기에서는 {0}을 누르고 있으면 마스크 창에 오버레이가, Shift+{0}이면 사진 창에 오버레이가 보입니다."),
    DE("Nebeneinander: {0} gedrückt halten zeigt die Überlagerung im Maskenfeld, Shift+{0} im Fotofeld."),
    FR("Côte à côte : maintenir {0} montre la superposition dans le volet du masque, Shift+{0} dans le volet de la photo."),
    ES("Lado a lado: mantén {0} para ver la superposición en el panel de la máscara, Shift+{0} en el panel de la foto."),
    PT("Lado a lado: mantenha {0} para ver a sobreposição no painel da máscara, Shift+{0} no painel da foto."),
    IT("Affiancate: tieni premuto {0} per vedere la sovrapposizione nel riquadro della maschera, Shift+{0} in quello della foto."),
    NL("Naast elkaar: houd {0} ingedrukt voor de overlay in het maskervak, Shift+{0} in het fotovak."),
    RU("Рядом: удерживайте {0}, чтобы увидеть наложение в панели маски, Shift+{0} в панели фото."),
    TR("Yan yana: {0} basılı tutulunca maske bölmesinde bindirme, Shift+{0} ile fotoğraf bölmesinde bindirme görünür."));

// ===========================================================================
// Propagate
// ===========================================================================

SS_MSG(prop_scope_next,
    EN("Next frame"),
    JA("次のフレーム"),
    ZH_HANS("下一帧"),
    ZH_HANT("下一影格"),
    KO("다음 프레임"),
    DE("Nächstes Bild"),
    FR("Image suivante"),
    ES("Fotograma siguiente"),
    PT("Quadro seguinte"),
    IT("Fotogramma successivo"),
    NL("Volgende frame"),
    RU("Следующий кадр"),
    TR("Sonraki kare"));

SS_MSG(prop_scope_range,
    EN("Range"),
    JA("範囲"),
    ZH_HANS("范围"),
    ZH_HANT("範圍"),
    KO("범위"),
    DE("Bereich"),
    FR("Plage"),
    ES("Rango"),
    PT("Intervalo"),
    IT("Intervallo"),
    NL("Bereik"),
    RU("Диапазон"),
    TR("Aralık"));

SS_MSG(prop_scope_camera,
    EN("Whole camera"),
    JA("カメラ全体"),
    ZH_HANS("整个相机"),
    ZH_HANT("整台相機"),
    KO("카메라 전체"),
    DE("Ganze Kamera"),
    FR("Toute la caméra"),
    ES("Toda la cámara"),
    PT("Toda a câmera"),
    IT("Tutta la fotocamera"),
    NL("Hele camera"),
    RU("Вся камера"),
    TR("Tüm kamera"));

SS_MSG(prop_from,
    EN("From"),
    JA("開始"),
    ZH_HANS("从"),
    ZH_HANT("從"),
    KO("시작"),
    DE("Von"),
    FR("De"),
    ES("Desde"),
    PT("De"),
    IT("Da"),
    NL("Van"),
    RU("С"),
    TR("Başlangıç"));

SS_MSG(prop_to,
    EN("To"),
    JA("終了"),
    ZH_HANS("到"),
    ZH_HANT("到"),
    KO("끝"),
    DE("Bis"),
    FR("À"),
    ES("Hasta"),
    PT("Até"),
    IT("A"),
    NL("Tot"),
    RU("По"),
    TR("Bitiş"));

SS_MSG(prop_go,
    EN("Propagate"),
    JA("伝播"),
    ZH_HANS("传播"),
    ZH_HANT("傳播"),
    KO("전파"),
    DE("Übertragen"),
    FR("Propager"),
    ES("Propagar"),
    PT("Propagar"),
    IT("Propaga"),
    NL("Doorvoeren"),
    RU("Распространить"),
    TR("Yay"));

SS_MSG(prop_go_help,
    EN("Copy this frame's corrections, not its mask, onto other frames of the same camera. They replace the corrections those frames already had; each keeps its own model output underneath. A frame whose mask has another size is refused. Edits made here afterwards, by hand or with SAM, are not carried over."),
    JA("このフレームの修正（マスクではなく）を同じカメラの他のフレームにコピーします。コピー先に既にある修正は置き換えられ、各フレームは修正の下に自身のモデル出力を保ちます。マスクのサイズが異なるフレームは拒否されます。この後ここで手作業や SAM で行った編集は反映されません。"),
    ZH_HANS("将此帧的修正（而非蒙版）复制到同一相机的其他帧。它们会替换这些帧原有的修正；每一帧在修正之下保留自己的模型输出。蒙版尺寸不同的帧会被拒绝。之后在此帧上手动或用 SAM 所做的编辑不会带过去。"),
    ZH_HANT("將此影格的修正（而非遮罩）複製到同一相機的其他影格。它們會取代這些影格原有的修正；每一影格在修正之下保留自己的模型輸出。遮罩尺寸不同的影格會被拒絕。之後在此影格上手動或用 SAM 所做的編輯不會帶過去。"),
    KO("이 프레임의 수정(마스크가 아님)을 같은 카메라의 다른 프레임에 복사합니다. 그 프레임들이 이미 가진 수정은 대체되며, 각 프레임은 수정 아래에 자신의 모델 출력을 유지합니다. 마스크 크기가 다른 프레임은 거부됩니다. 이후 여기서 손으로 또는 SAM으로 한 편집은 전달되지 않습니다."),
    DE("Die Korrekturen dieses Bildes, nicht seine Maske, auf andere Bilder derselben Kamera übertragen. Sie ersetzen die Korrekturen, die diese Bilder schon hatten; jedes behält darunter seine eigene Modellausgabe. Ein Bild mit einer anders großen Maske wird abgelehnt. Spätere Änderungen hier, von Hand oder mit SAM, werden nicht mitgenommen."),
    FR("Copie les corrections de cette image, pas son masque, sur d'autres images de la même caméra. Elles remplacent les corrections que ces images avaient déjà ; chacune garde dessous sa propre sortie du modèle. Une image dont le masque a une autre taille est refusée. Les modifications faites ici ensuite, à la main ou avec SAM, ne suivent pas."),
    ES("Copia las correcciones de este fotograma, no su máscara, a otros fotogramas de la misma cámara. Sustituyen las correcciones que esos fotogramas ya tenían; cada uno conserva debajo su propia salida del modelo. Un fotograma cuya máscara tenga otro tamaño se rechaza. Las ediciones hechas aquí después, a mano o con SAM, no se trasladan."),
    PT("Copia as correções deste quadro, não a sua máscara, para outros quadros da mesma câmera. Substituem as correções que esses quadros já tinham; cada um mantém por baixo a sua própria saída do modelo. Um quadro cuja máscara tenha outro tamanho é recusado. As edições feitas aqui depois, à mão ou com o SAM, não são levadas."),
    IT("Copia le correzioni di questo fotogramma, non la sua maschera, su altri fotogrammi della stessa fotocamera. Sostituiscono le correzioni che quei fotogrammi avevano già; ognuno conserva sotto la propria uscita del modello. Un fotogramma con una maschera di altra dimensione viene rifiutato. Le modifiche fatte qui in seguito, a mano o con SAM, non vengono riportate."),
    NL("Kopieert de correcties van dit frame, niet het masker, naar andere frames van dezelfde camera. Ze vervangen de correcties die die frames al hadden; elk houdt eronder zijn eigen modeluitvoer. Een frame met een masker van een andere grootte wordt geweigerd. Latere bewerkingen hier, met de hand of met SAM, gaan niet mee."),
    RU("Копирует исправления этого кадра, но не его маску, на другие кадры той же камеры. Они заменяют исправления, которые у этих кадров уже были; каждый сохраняет под ними собственный результат модели. Кадр с маской другого размера отклоняется. Последующие правки здесь, вручную или через SAM, не переносятся."),
    TR("Bu karenin düzeltmelerini (maskesini değil) aynı kameranın diğer karelerine kopyalar. O karelerin zaten sahip olduğu düzeltmelerin yerini alır; her kare altında kendi model çıktısını korur. Maskesi farklı boyutta olan kare reddedilir. Sonradan burada elle veya SAM ile yapılan düzenlemeler aktarılmaz."));

SS_MSG(prop_warn_moves,
    EN("Propagate copies pixels: it does not follow an object that moves between frames."),
    JA("伝播はピクセルをコピーします。フレーム間で動く物体には追従しません。"),
    ZH_HANS("传播复制的是像素：不会跟随在帧之间移动的物体。"),
    ZH_HANT("傳播複製的是像素：不會跟隨在影格之間移動的物體。"),
    KO("전파는 픽셀을 복사합니다. 프레임 사이에서 움직이는 물체는 따라가지 않습니다."),
    DE("Übertragen kopiert Pixel: einem Objekt, das sich zwischen Bildern bewegt, folgt es nicht."),
    FR("La propagation copie des pixels : elle ne suit pas un objet qui se déplace d'une image à l'autre."),
    ES("Propagar copia píxeles: no sigue a un objeto que se mueve entre fotogramas."),
    PT("Propagar copia pixels: não segue um objeto que se move entre quadros."),
    IT("La propagazione copia i pixel: non segue un oggetto che si sposta tra i fotogrammi."),
    NL("Doorvoeren kopieert pixels: een object dat tussen frames beweegt, wordt niet gevolgd."),
    RU("Распространение копирует пиксели: объект, который движется между кадрами, не отслеживается."),
    TR("Yayma pikselleri kopyalar: kareler arasında hareket eden bir nesneyi izlemez."));

SS_MSG(prop_undo,
    EN("Undo propagate"),
    JA("伝播を元に戻す"),
    ZH_HANS("撤销传播"),
    ZH_HANT("復原傳播"),
    KO("전파 취소"),
    DE("Übertragen rückgängig"),
    FR("Annuler la propagation"),
    ES("Deshacer propagación"),
    PT("Desfazer propagação"),
    IT("Annulla propagazione"),
    NL("Doorvoeren ongedaan maken"),
    RU("Отменить распространение"),
    TR("Yaymayı geri al"));

SS_MSG(prop_undo_help,
    EN("Put every frame the last propagate touched back as it was. Offered while this frame is still the one it was propagated from."),
    JA("直前の伝播が変更したすべてのフレームを元の状態に戻します。伝播元のフレームを表示している間のみ使えます。"),
    ZH_HANS("将上次传播改动的所有帧恢复原状。仅当当前帧仍是传播源时可用。"),
    ZH_HANT("將上次傳播改動的所有影格恢復原狀。僅當目前影格仍是傳播來源時可用。"),
    KO("마지막 전파가 바꾼 모든 프레임을 원래대로 되돌립니다. 이 프레임이 여전히 전파의 원본일 때만 제공됩니다."),
    DE("Jedes Bild, das das letzte Übertragen geändert hat, wieder in den vorherigen Zustand bringen. Nur solange dieses Bild noch das Ausgangsbild ist."),
    FR("Remet chaque image touchée par la dernière propagation dans son état antérieur. Proposé tant que cette image est encore celle d'origine."),
    ES("Devuelve cada fotograma que tocó la última propagación a su estado anterior. Disponible mientras este fotograma siga siendo el de origen."),
    PT("Repõe cada quadro tocado pela última propagação no estado anterior. Disponível enquanto este quadro ainda for o de origem."),
    IT("Riporta ogni fotogramma toccato dall'ultima propagazione allo stato precedente. Disponibile finché questo fotogramma è ancora quello di origine."),
    NL("Zet elk frame dat het laatste doorvoeren raakte terug zoals het was. Beschikbaar zolang dit frame nog het bronframe is."),
    RU("Возвращает каждый кадр, затронутый последним распространением, в прежнее состояние. Доступно, пока этот кадр остаётся исходным."),
    TR("Son yaymanın değiştirdiği her kareyi eski haline döndürür. Bu kare hâlâ kaynak kare olduğu sürece sunulur."));

SS_MSG(prop_done,
    EN("Propagated: {0}, refused: {1}, failed: {2}"),
    JA("伝播済み: {0}、拒否: {1}、失敗: {2}"),
    ZH_HANS("已传播：{0}，已拒绝：{1}，失败：{2}"),
    ZH_HANT("已傳播：{0}，已拒絕：{1}，失敗：{2}"),
    KO("전파됨: {0}, 거부됨: {1}, 실패: {2}"),
    DE("Übertragen: {0}, abgelehnt: {1}, fehlgeschlagen: {2}"),
    FR("Propagées : {0}, refusées : {1}, échouées : {2}"),
    ES("Propagados: {0}, rechazados: {1}, fallidos: {2}"),
    PT("Propagados: {0}, recusados: {1}, com falha: {2}"),
    IT("Propagati: {0}, rifiutati: {1}, falliti: {2}"),
    NL("Doorgevoerd: {0}, geweigerd: {1}, mislukt: {2}"),
    RU("Распространено: {0}, отклонено: {1}, не удалось: {2}"),
    TR("Yayıldı: {0}, reddedildi: {1}, başarısız: {2}"));

SS_MSG(prop_failed_restored,
    EN("Failed on {0}: {1} could not be read or written. That frame was left as it was."),
    JA("{0} で失敗: {1} を読み書きできませんでした。そのフレームは元のままです。"),
    ZH_HANS("{0} 失败：无法读写 {1}。该帧保持原状。"),
    ZH_HANT("{0} 失敗：無法讀寫 {1}。該影格保持原狀。"),
    KO("{0}에서 실패: {1}을(를) 읽거나 쓸 수 없습니다. 해당 프레임은 원래대로 두었습니다."),
    DE("Fehler bei {0}: {1} konnte nicht gelesen oder geschrieben werden. Dieses Bild blieb, wie es war."),
    FR("Échec sur {0} : impossible de lire ou d'écrire {1}. Cette image est restée telle quelle."),
    ES("Fallo en {0}: no se pudo leer ni escribir {1}. Ese fotograma quedó como estaba."),
    PT("Falha em {0}: não foi possível ler nem escrever {1}. Esse quadro ficou como estava."),
    IT("Errore su {0}: impossibile leggere o scrivere {1}. Quel fotogramma è rimasto com'era."),
    NL("Mislukt bij {0}: kon {1} niet lezen of schrijven. Dat frame is gebleven zoals het was."),
    RU("Сбой на {0}: не удалось прочитать или записать {1}. Этот кадр оставлен как был."),
    TR("{0} üzerinde başarısız: {1} okunamadı veya yazılamadı. O kare olduğu gibi bırakıldı."));

SS_MSG(prop_failed_not_restored,
    EN("Failed on {0}: {1} could not be written, and the frame could not be put back. Frames not put back: {2}. Use Undo propagate to try again."),
    JA("{0} で失敗: {1} を書き込めず、そのフレームを元に戻せませんでした。元に戻せなかったフレーム: {2}。「伝播を元に戻す」で再試行してください。"),
    ZH_HANS("{0} 失败：无法写入 {1}，且该帧未能恢复。未能恢复的帧：{2}。使用“撤销传播”重试。"),
    ZH_HANT("{0} 失敗：無法寫入 {1}，且該影格未能恢復。未能恢復的影格：{2}。使用「復原傳播」重試。"),
    KO("{0}에서 실패: {1}을(를) 쓸 수 없고 해당 프레임을 되돌리지 못했습니다. 되돌리지 못한 프레임: {2}. '전파 취소'로 다시 시도하세요."),
    DE("Fehler bei {0}: {1} konnte nicht geschrieben und das Bild nicht zurückgesetzt werden. Nicht zurückgesetzte Bilder: {2}. Mit „Übertragen rückgängig“ erneut versuchen."),
    FR("Échec sur {0} : impossible d'écrire {1}, et l'image n'a pas pu être remise en état. Images non remises en état : {2}. Utilisez « Annuler la propagation » pour réessayer."),
    ES("Fallo en {0}: no se pudo escribir {1} y el fotograma no se pudo restaurar. Fotogramas sin restaurar: {2}. Use «Deshacer propagación» para reintentar."),
    PT("Falha em {0}: não foi possível escrever {1} e o quadro não pôde ser restaurado. Quadros não restaurados: {2}. Use \"Desfazer propagação\" para tentar de novo."),
    IT("Errore su {0}: impossibile scrivere {1} e il fotogramma non è stato ripristinato. Fotogrammi non ripristinati: {2}. Usa «Annulla propagazione» per riprovare."),
    NL("Mislukt bij {0}: kon {1} niet schrijven en het frame kon niet worden teruggezet. Niet teruggezette frames: {2}. Gebruik 'Doorvoeren ongedaan maken' om het opnieuw te proberen."),
    RU("Сбой на {0}: не удалось записать {1}, и кадр не удалось восстановить. Не восстановлено кадров: {2}. Нажмите «Отменить распространение», чтобы повторить попытку."),
    TR("{0} üzerinde başarısız: {1} yazılamadı ve kare geri alınamadı. Geri alınamayan kare sayısı: {2}. Yeniden denemek için 'Yaymayı geri al'ı kullanın."));

SS_MSG(prop_failed_not_restored_final,
    EN("Failed on {0}: {1} could not be written, and the frame could not be put back. Frames not put back: {2}. The propagate was too large to record, so it cannot be undone."),
    JA("{0} で失敗: {1} を書き込めず、そのフレームを元に戻せませんでした。元に戻せなかったフレーム: {2}。伝播の記録が大きすぎるため、元に戻せません。"),
    ZH_HANS("{0} 失败：无法写入 {1}，且该帧未能恢复。未能恢复的帧：{2}。传播记录过大，无法撤销。"),
    ZH_HANT("{0} 失敗：無法寫入 {1}，且該影格未能恢復。未能恢復的影格：{2}。傳播記錄過大，無法復原。"),
    KO("{0}에서 실패: {1}을(를) 쓸 수 없고 해당 프레임을 되돌리지 못했습니다. 되돌리지 못한 프레임: {2}. 전파 기록이 너무 커서 취소할 수 없습니다."),
    DE("Fehler bei {0}: {1} konnte nicht geschrieben und das Bild nicht zurückgesetzt werden. Nicht zurückgesetzte Bilder: {2}. Die Übertragung war zu groß zum Aufzeichnen und lässt sich nicht rückgängig machen."),
    FR("Échec sur {0} : impossible d'écrire {1}, et l'image n'a pas pu être remise en état. Images non remises en état : {2}. La propagation était trop grosse pour être enregistrée et ne peut pas être annulée."),
    ES("Fallo en {0}: no se pudo escribir {1} y el fotograma no se pudo restaurar. Fotogramas sin restaurar: {2}. La propagación era demasiado grande para registrarla y no se puede deshacer."),
    PT("Falha em {0}: não foi possível escrever {1} e o quadro não pôde ser restaurado. Quadros não restaurados: {2}. A propagação era grande demais para ser registrada e não pode ser desfeita."),
    IT("Errore su {0}: impossibile scrivere {1} e il fotogramma non è stato ripristinato. Fotogrammi non ripristinati: {2}. La propagazione era troppo grande per essere registrata e non si può annullare."),
    NL("Mislukt bij {0}: kon {1} niet schrijven en het frame kon niet worden teruggezet. Niet teruggezette frames: {2}. Het doorvoeren was te groot om vast te leggen en kan niet ongedaan worden gemaakt."),
    RU("Сбой на {0}: не удалось записать {1}, и кадр не удалось восстановить. Не восстановлено кадров: {2}. Распространение слишком велико для записи, его нельзя отменить."),
    TR("{0} üzerinde başarısız: {1} yazılamadı ve kare geri alınamadı. Geri alınamayan kare sayısı: {2}. Yayma kaydedilemeyecek kadar büyüktü, geri alınamaz."));

SS_MSG(prop_refused_size,
    EN("Refused {0}: its mask is {1} x {2}, this frame's is {3} x {4}."),
    JA("{0} を拒否しました: そのマスクは {1} x {2}、このフレームは {3} x {4} です。"),
    ZH_HANS("已拒绝 {0}：其蒙版为 {1} x {2}，此帧为 {3} x {4}。"),
    ZH_HANT("已拒絕 {0}：其遮罩為 {1} x {2}，此影格為 {3} x {4}。"),
    KO("{0} 거부됨: 그 마스크는 {1} x {2}, 이 프레임은 {3} x {4}입니다."),
    DE("{0} abgelehnt: seine Maske ist {1} x {2}, die dieses Bildes {3} x {4}."),
    FR("{0} refusée : son masque fait {1} x {2}, celui de cette image {3} x {4}."),
    ES("{0} rechazado: su máscara es de {1} x {2}, la de este fotograma de {3} x {4}."),
    PT("{0} recusado: a sua máscara é {1} x {2}, a deste quadro é {3} x {4}."),
    IT("{0} rifiutato: la sua maschera è {1} x {2}, quella di questo fotogramma {3} x {4}."),
    NL("{0} geweigerd: zijn masker is {1} x {2}, dat van dit frame {3} x {4}."),
    RU("{0} отклонён: его маска {1} x {2}, а у этого кадра {3} x {4}."),
    TR("{0} reddedildi: maskesi {1} x {2}, bu karenin maskesi {3} x {4}."));

SS_MSG(prop_no_targets,
    EN("No other frame of this camera to propagate to."),
    JA("伝播先となる同じカメラの他のフレームがありません。"),
    ZH_HANS("同一相机没有其他可传播的帧。"),
    ZH_HANT("同一相機沒有其他可傳播的影格。"),
    KO("전파할 이 카메라의 다른 프레임이 없습니다."),
    DE("Kein anderes Bild dieser Kamera zum Übertragen."),
    FR("Aucune autre image de cette caméra sur laquelle propager."),
    ES("No hay otro fotograma de esta cámara al que propagar."),
    PT("Não há outro quadro desta câmera para onde propagar."),
    IT("Nessun altro fotogramma di questa fotocamera su cui propagare."),
    NL("Geen ander frame van deze camera om naar door te voeren."),
    RU("Нет других кадров этой камеры, на которые можно распространить."),
    TR("Bu kameranın yayılacak başka karesi yok."));

SS_MSG(prop_range_empty,
    EN("The range is empty: From is after To."),
    JA("範囲が空です：「開始」が「終了」より後にあります。"),
    ZH_HANS("范围为空：“从”在“到”之后。"),
    ZH_HANT("範圍為空：「從」在「到」之後。"),
    KO("범위가 비어 있습니다: '시작'이 '끝'보다 뒤에 있습니다."),
    DE("Der Bereich ist leer: „Von“ liegt nach „Bis“."),
    FR("La plage est vide : « De » est après « À »."),
    ES("El rango está vacío: «Desde» es posterior a «Hasta»."),
    PT("O intervalo está vazio: \"De\" vem depois de \"Até\"."),
    IT("L'intervallo è vuoto: «Da» viene dopo «A»."),
    NL("Het bereik is leeg: 'Van' ligt na 'Tot'."),
    RU("Диапазон пуст: «С» идёт после «По»."),
    TR("Aralık boş: 'Başlangıç', 'Bitiş'ten sonra."));

SS_MSG(prop_not_undoable,
    EN("Propagated, but the record is too large to undo: {0} MB."),
    JA("伝播しましたが、記録が大きすぎて元に戻せません: {0} MB。"),
    ZH_HANS("已传播，但记录过大无法撤销：{0} MB。"),
    ZH_HANT("已傳播，但記錄過大無法復原：{0} MB。"),
    KO("전파되었지만 기록이 너무 커서 취소할 수 없습니다: {0} MB."),
    DE("Übertragen, aber die Aufzeichnung ist zu groß zum Rückgängigmachen: {0} MB."),
    FR("Propagé, mais l'enregistrement est trop gros pour être annulé : {0} Mo."),
    ES("Propagado, pero el registro es demasiado grande para deshacer: {0} MB."),
    PT("Propagado, mas o registro é grande demais para desfazer: {0} MB."),
    IT("Propagato, ma la registrazione è troppo grande per essere annullata: {0} MB."),
    NL("Doorgevoerd, maar de opname is te groot om ongedaan te maken: {0} MB."),
    RU("Распространено, но запись слишком велика для отмены: {0} МБ."),
    TR("Yayıldı, ancak kayıt geri alınamayacak kadar büyük: {0} MB."));

SS_MSG(prop_working,
    EN("Propagating..."),
    JA("伝播中…"),
    ZH_HANS("正在传播…"),
    ZH_HANT("正在傳播…"),
    KO("전파 중…"),
    DE("Wird übertragen …"),
    FR("Propagation…"),
    ES("Propagando…"),
    PT("Propagando…"),
    IT("Propagazione…"),
    NL("Bezig met doorvoeren…"),
    RU("Распространение…"),
    TR("Yayılıyor…"));

SS_MSG(prop_progress,
    EN("Propagating: {0} / {1} frames"),
    JA("伝播中: {0} / {1} フレーム"),
    ZH_HANS("正在传播：{0} / {1} 帧"),
    ZH_HANT("正在傳播：{0} / {1} 影格"),
    KO("전파 중: {0} / {1} 프레임"),
    DE("Wird übertragen: {0} / {1} Bilder"),
    FR("Propagation : {0} / {1} images"),
    ES("Propagando: {0} / {1} fotogramas"),
    PT("Propagando: {0} / {1} quadros"),
    IT("Propagazione: {0} / {1} fotogrammi"),
    NL("Bezig met doorvoeren: {0} / {1} beelden"),
    RU("Распространение: {0} / {1} кадров"),
    TR("Yayılıyor: {0} / {1} kare"));

SS_MSG(prop_stopped,
    EN("Propagate stopped: {0} done, {1} not reached, refused: {2}, failed: {3}"),
    JA("伝播を中止しました: 完了 {0}、未処理 {1}、拒否: {2}、失敗: {3}"),
    ZH_HANS("已停止传播：完成 {0}，未处理 {1}，已拒绝：{2}，失败：{3}"),
    ZH_HANT("已停止傳播：完成 {0}，未處理 {1}，已拒絕：{2}，失敗：{3}"),
    KO("전파 중지됨: 완료 {0}, 처리 안 됨 {1}, 거부됨: {2}, 실패: {3}"),
    DE("Übertragen abgebrochen: {0} fertig, {1} nicht erreicht, abgelehnt: {2}, fehlgeschlagen: {3}"),
    FR("Propagation arrêtée : {0} faites, {1} non atteintes, refusées : {2}, échouées : {3}"),
    ES("Propagación detenida: {0} hechos, {1} sin alcanzar, rechazados: {2}, fallidos: {3}"),
    PT("Propagação interrompida: {0} feitos, {1} não alcançados, recusados: {2}, com falha: {3}"),
    IT("Propagazione interrotta: {0} fatti, {1} non raggiunti, rifiutati: {2}, falliti: {3}"),
    NL("Doorvoeren gestopt: {0} klaar, {1} niet bereikt, geweigerd: {2}, mislukt: {3}"),
    RU("Распространение остановлено: готово {0}, не обработано {1}, отклонено: {2}, не удалось: {3}"),
    TR("Yayma durduruldu: {0} tamam, {1} işlenmedi, reddedildi: {2}, başarısız: {3}"));

SS_MSG(prop_undo_stopped,
    EN("Undo propagate stopped: {0} put back, {1} still propagated. Undo propagate again to finish."),
    JA("伝播の取り消しを中止しました: {0} を戻し、{1} は伝播したままです。もう一度「伝播を元に戻す」で完了します。"),
    ZH_HANS("撤销传播已停止：已恢复 {0}，仍有 {1} 保持传播。再次撤销传播即可完成。"),
    ZH_HANT("復原傳播已停止：已還原 {0}，仍有 {1} 保持傳播。再次復原傳播即可完成。"),
    KO("전파 취소 중지됨: {0}개 복원, {1}개는 아직 전파된 상태입니다. 전파 취소를 다시 눌러 마치세요."),
    DE("Rückgängig abgebrochen: {0} wiederhergestellt, {1} noch übertragen. Erneut rückgängig machen, um abzuschließen."),
    FR("Annulation arrêtée : {0} rétablies, {1} encore propagées. Annulez la propagation à nouveau pour terminer."),
    ES("Deshacer detenido: {0} restaurados, {1} siguen propagados. Deshaga la propagación otra vez para terminar."),
    PT("Desfazer interrompido: {0} restaurados, {1} ainda propagados. Desfaça a propagação de novo para concluir."),
    IT("Annullamento interrotto: {0} ripristinati, {1} ancora propagati. Annulla di nuovo la propagazione per finire."),
    NL("Ongedaan maken gestopt: {0} teruggezet, {1} nog doorgevoerd. Maak het doorvoeren opnieuw ongedaan om af te ronden."),
    RU("Отмена остановлена: восстановлено {0}, ещё распространено {1}. Отмените распространение снова, чтобы завершить."),
    TR("Geri alma durduruldu: {0} geri kondu, {1} hâlâ yayılmış. Bitirmek için yaymayı yeniden geri alın."));

SS_MSG(prop_cancel,
    EN("Stop"),
    JA("中止"),
    ZH_HANS("停止"),
    ZH_HANT("停止"),
    KO("중지"),
    DE("Abbrechen"),
    FR("Arrêter"),
    ES("Detener"),
    PT("Parar"),
    IT("Interrompi"),
    NL("Stoppen"),
    RU("Остановить"),
    TR("Durdur"));

SS_MSG(prop_cancel_help,
    EN("Stop after the frames being written now. Frames already done stay done, and Undo propagate still takes them back."),
    JA("書き込み中のフレームの後で中止します。完了したフレームはそのまま残り、「伝播を元に戻す」で戻せます。"),
    ZH_HANS("在当前正在写入的帧完成后停止。已完成的帧保持不变，仍可用撤销传播恢复。"),
    ZH_HANT("在目前正在寫入的影格完成後停止。已完成的影格保持不變，仍可用復原傳播還原。"),
    KO("지금 쓰는 프레임까지만 마치고 중지합니다. 이미 끝난 프레임은 그대로 두며, 전파 취소로 되돌릴 수 있습니다."),
    DE("Nach den gerade geschriebenen Bildern anhalten. Fertige Bilder bleiben erhalten; Übertragen rückgängig nimmt sie weiterhin zurück."),
    FR("S'arrêter après les images en cours d'écriture. Les images terminées le restent, et Annuler la propagation les rétablit toujours."),
    ES("Detenerse tras los fotogramas que se escriben ahora. Los terminados se quedan así, y Deshacer propagación aún los revierte."),
    PT("Parar depois dos quadros sendo gravados agora. Os já feitos permanecem, e Desfazer propagação ainda os reverte."),
    IT("Fermarsi dopo i fotogrammi in scrittura. Quelli già fatti restano, e Annulla propagazione li ripristina comunque."),
    NL("Stoppen na de beelden die nu worden geschreven. Klare beelden blijven zo, en Doorvoeren ongedaan maken zet ze nog steeds terug."),
    RU("Остановиться после кадров, которые записываются сейчас. Готовые кадры остаются, и отмена распространения по-прежнему их вернёт."),
    TR("Şu an yazılan karelerden sonra dur. Biten kareler öyle kalır; Yaymayı geri al onları yine geri alır."));

SS_MSG(prop_undone,
    EN("Propagate undone: {0}"),
    JA("伝播を元に戻しました: {0}"),
    ZH_HANS("已撤销传播：{0}"),
    ZH_HANT("已復原傳播：{0}"),
    KO("전파 취소됨: {0}"),
    DE("Übertragen rückgängig gemacht: {0}"),
    FR("Propagation annulée : {0}"),
    ES("Propagación deshecha: {0}"),
    PT("Propagação desfeita: {0}"),
    IT("Propagazione annullata: {0}"),
    NL("Doorvoeren ongedaan gemaakt: {0}"),
    RU("Распространение отменено: {0}"),
    TR("Yayma geri alındı: {0}"));

SS_MSG(prop_undo_failed,
    EN("Could not put {0} back: {1} could not be read or written. Use Undo propagate to try again."),
    JA("{0} を元に戻せませんでした: {1} を読み書きできませんでした。「伝播を元に戻す」で再試行してください。"),
    ZH_HANS("无法恢复 {0}：无法读写 {1}。使用“撤销传播”重试。"),
    ZH_HANT("無法恢復 {0}：無法讀寫 {1}。使用「復原傳播」重試。"),
    KO("{0}을(를) 되돌리지 못했습니다: {1}을(를) 읽거나 쓸 수 없습니다. '전파 취소'로 다시 시도하세요."),
    DE("{0} konnte nicht zurückgesetzt werden: {1} konnte nicht gelesen oder geschrieben werden. Mit „Übertragen rückgängig“ erneut versuchen."),
    FR("Impossible de remettre {0} en état : impossible de lire ou d'écrire {1}. Utilisez « Annuler la propagation » pour réessayer."),
    ES("No se pudo restaurar {0}: no se pudo leer ni escribir {1}. Use «Deshacer propagación» para reintentar."),
    PT("Não foi possível restaurar {0}: não foi possível ler nem escrever {1}. Use \"Desfazer propagação\" para tentar de novo."),
    IT("Impossibile ripristinare {0}: impossibile leggere o scrivere {1}. Usa «Annulla propagazione» per riprovare."),
    NL("Kon {0} niet terugzetten: kon {1} niet lezen of schrijven. Gebruik 'Doorvoeren ongedaan maken' om het opnieuw te proberen."),
    RU("Не удалось восстановить {0}: не удалось прочитать или записать {1}. Нажмите «Отменить распространение», чтобы повторить попытку."),
    TR("{0} geri alınamadı: {1} okunamadı veya yazılamadı. Yeniden denemek için 'Yaymayı geri al'ı kullanın."));

SS_MSG(prop_failed_stray_base,
    EN("Skipped {0}: {1} is left over from an unfinished save of that frame, so a propagate there could not be undone. That frame was left as it was."),
    JA("{0} をスキップしました: {1} はそのフレームの保存が途中で終わった名残のため、そこへの伝播は元に戻せません。そのフレームは元のままです。"),
    ZH_HANS("已跳过 {0}：{1} 是该帧一次未完成的保存留下的，在此传播将无法撤销。该帧保持原状。"),
    ZH_HANT("已略過 {0}：{1} 是該影格一次未完成的儲存所留下的，在此傳播將無法復原。該影格保持原狀。"),
    KO("{0} 건너뜀: {1}은(는) 그 프레임의 저장이 끝나지 않아 남은 파일이라 그곳으로의 전파는 취소할 수 없습니다. 해당 프레임은 원래대로 두었습니다."),
    DE("{0} übersprungen: {1} ist von einem unvollständigen Speichern dieses Bildes übrig, daher ließe sich ein Übertragen dorthin nicht rückgängig machen. Dieses Bild blieb, wie es war."),
    FR("{0} ignorée : {1} reste d'un enregistrement inachevé de cette image, une propagation n'y serait donc pas annulable. Cette image est restée telle quelle."),
    ES("{0} omitido: {1} quedó de un guardado sin terminar de ese fotograma, así que una propagación ahí no se podría deshacer. Ese fotograma quedó como estaba."),
    PT("{0} ignorado: {1} sobrou de um salvamento inacabado desse quadro, então uma propagação ali não poderia ser desfeita. Esse quadro ficou como estava."),
    IT("{0} saltato: {1} è rimasto da un salvataggio incompiuto di quel fotogramma, quindi una propagazione lì non si potrebbe annullare. Quel fotogramma è rimasto com'era."),
    NL("{0} overgeslagen: {1} is overgebleven van een onvoltooide opslag van dat frame, dus doorvoeren daarheen zou niet ongedaan te maken zijn. Dat frame is gebleven zoals het was."),
    RU("{0} пропущен: {1} остался от незавершённого сохранения этого кадра, поэтому распространение на него нельзя было бы отменить. Этот кадр оставлен как был."),
    TR("{0} atlandı: {1} o karenin yarım kalmış bir kaydından arta kaldı, bu yüzden oraya yayma geri alınamazdı. O kare olduğu gibi bırakıldı."));

// ===========================================================================
// Find missing
// ===========================================================================

SS_MSG(find_first,
    EN("First"),
    JA("最初"),
    ZH_HANS("第一帧"),
    ZH_HANT("第一格"),
    KO("처음"),
    DE("Erstes"),
    FR("Première"),
    ES("Primero"),
    PT("Primeiro"),
    IT("Primo"),
    NL("Eerste"),
    RU("Первый"),
    TR("İlk"));

SS_MSG(find_last,
    EN("Last"),
    JA("最後"),
    ZH_HANS("最后一帧"),
    ZH_HANT("最後一格"),
    KO("마지막"),
    DE("Letztes"),
    FR("Dernière"),
    ES("Último"),
    PT("Último"),
    IT("Ultimo"),
    NL("Laatste"),
    RU("Последний"),
    TR("Son"));

SS_MSG(find_prev,
    EN("Previous missing"),
    JA("前の欠落"),
    ZH_HANS("上一个缺失"),
    ZH_HANT("上一個缺失"),
    KO("이전 누락"),
    DE("Vorheriges fehlendes"),
    FR("Manquante précédente"),
    ES("Anterior faltante"),
    PT("Anterior faltante"),
    IT("Mancante precedente"),
    NL("Vorige ontbrekende"),
    RU("Предыдущий пропуск"),
    TR("Önceki eksik"));

SS_MSG(find_next,
    EN("Next missing"),
    JA("次の欠落"),
    ZH_HANS("下一个缺失"),
    ZH_HANT("下一個缺失"),
    KO("다음 누락"),
    DE("Nächstes fehlendes"),
    FR("Manquante suivante"),
    ES("Siguiente faltante"),
    PT("Próximo faltante"),
    IT("Mancante successivo"),
    NL("Volgende ontbrekende"),
    RU("Следующий пропуск"),
    TR("Sonraki eksik"));

SS_MSG(find_help,
    EN("A frame is missing when it has no mask file, or when its kept fraction lies outside the band. Near 0 is an inverted mask; near 100 is a mask that masked nothing. M and Shift+M jump to the next and previous one."),
    JA("マスクファイルがない、または保持率が範囲外のフレームを欠落とみなします。0 付近は反転したマスク、100 付近は何もマスクしていないマスクです。M と Shift+M で次と前の欠落へ移動します。"),
    ZH_HANS("没有蒙版文件、或保留比例超出范围的帧视为缺失。接近 0 是反转的蒙版；接近 100 是什么都没遮住的蒙版。M 和 Shift+M 跳到下一个和上一个。"),
    ZH_HANT("沒有遮罩檔案、或保留比例超出範圍的影格視為缺失。接近 0 是反轉的遮罩；接近 100 是什麼都沒遮住的遮罩。M 和 Shift+M 跳到下一個和上一個。"),
    KO("마스크 파일이 없거나 유지 비율이 범위를 벗어난 프레임을 누락으로 봅니다. 0에 가까우면 반전된 마스크, 100에 가까우면 아무것도 가리지 않은 마스크입니다. M과 Shift+M으로 다음과 이전으로 이동합니다."),
    DE("Ein Bild fehlt, wenn es keine Maskendatei hat oder sein behaltener Anteil außerhalb des Bandes liegt. Nahe 0 ist eine invertierte Maske; nahe 100 eine Maske, die nichts maskiert hat. M und Shift+M springen zum nächsten und vorherigen."),
    FR("Une image manque quand elle n'a pas de fichier de masque, ou quand sa fraction conservée sort de la bande. Près de 0, c'est un masque inversé ; près de 100, un masque qui n'a rien masqué. M et Shift+M sautent à la suivante et à la précédente."),
    ES("Un fotograma falta cuando no tiene archivo de máscara o cuando su fracción conservada queda fuera de la banda. Cerca de 0 es una máscara invertida; cerca de 100, una máscara que no enmascaró nada. M y Shift+M saltan al siguiente y al anterior."),
    PT("Um quadro está faltando quando não tem arquivo de máscara ou quando a sua fração mantida sai da banda. Perto de 0 é uma máscara invertida; perto de 100, uma máscara que não mascarou nada. M e Shift+M vão para o próximo e o anterior."),
    IT("Un fotogramma manca quando non ha un file di maschera o quando la sua frazione mantenuta è fuori dalla banda. Vicino a 0 è una maschera invertita; vicino a 100 una maschera che non ha mascherato nulla. M e Shift+M saltano al successivo e al precedente."),
    NL("Een frame ontbreekt als het geen maskerbestand heeft of als zijn behouden deel buiten de band valt. Dicht bij 0 is een omgekeerd masker; dicht bij 100 een masker dat niets maskeerde. M en Shift+M springen naar de volgende en vorige."),
    RU("Кадр считается пропущенным, если у него нет файла маски или доля оставленного выходит за полосу. Около 0 это инвертированная маска; около 100 это маска, которая ничего не скрыла. M и Shift+M переходят к следующему и предыдущему."),
    TR("Maske dosyası olmayan veya tutulan oranı bandın dışında kalan kare eksik sayılır. 0'a yakın ters çevrilmiş maske, 100'e yakın hiçbir şeyi maskelememiş maskedir. M ve Shift+M sonrakine ve öncekine atlar."));

SS_MSG(find_band_lo,
    EN("Min kept %"),
    JA("保持率の下限 %"),
    ZH_HANS("最小保留 %"),
    ZH_HANT("最小保留 %"),
    KO("최소 유지 %"),
    DE("Min. behalten %"),
    FR("Conservé min. %"),
    ES("Mín. conservado %"),
    PT("Mín. mantido %"),
    IT("Min. mantenuto %"),
    NL("Min. behouden %"),
    RU("Мин. оставлено %"),
    TR("Min. tutulan %"));

SS_MSG(find_band_hi,
    EN("Max kept %"),
    JA("保持率の上限 %"),
    ZH_HANS("最大保留 %"),
    ZH_HANT("最大保留 %"),
    KO("최대 유지 %"),
    DE("Max. behalten %"),
    FR("Conservé max. %"),
    ES("Máx. conservado %"),
    PT("Máx. mantido %"),
    IT("Max. mantenuto %"),
    NL("Max. behouden %"),
    RU("Макс. оставлено %"),
    TR("Maks. tutulan %"));

SS_MSG(find_count,
    EN("Missing: {0}"),
    JA("欠落: {0}"),
    ZH_HANS("缺失：{0}"),
    ZH_HANT("缺失：{0}"),
    KO("누락: {0}"),
    DE("Fehlend: {0}"),
    FR("Manquantes : {0}"),
    ES("Faltantes: {0}"),
    PT("Em falta: {0}"),
    IT("Mancanti: {0}"),
    NL("Ontbrekend: {0}"),
    RU("Пропущено: {0}"),
    TR("Eksik: {0}"));

SS_MSG(find_scanning,
    EN("Scanning masks: {0} / {1}"),
    JA("マスクを走査中: {0} / {1}"),
    ZH_HANS("正在扫描蒙版：{0} / {1}"),
    ZH_HANT("正在掃描遮罩：{0} / {1}"),
    KO("마스크 검사 중: {0} / {1}"),
    DE("Masken werden geprüft: {0} / {1}"),
    FR("Analyse des masques : {0} / {1}"),
    ES("Examinando máscaras: {0} / {1}"),
    PT("Examinando máscaras: {0} / {1}"),
    IT("Scansione delle maschere: {0} / {1}"),
    NL("Maskers worden gescand: {0} / {1}"),
    RU("Проверка масок: {0} / {1}"),
    TR("Maskeler taranıyor: {0} / {1}"));

SS_MSG(find_none,
    EN("No missing frame in that direction."),
    JA("その方向に欠落フレームはありません。"),
    ZH_HANS("该方向没有缺失的帧。"),
    ZH_HANT("該方向沒有缺失的影格。"),
    KO("그 방향에는 누락된 프레임이 없습니다."),
    DE("Kein fehlendes Bild in dieser Richtung."),
    FR("Aucune image manquante dans cette direction."),
    ES("No hay fotograma faltante en esa dirección."),
    PT("Nenhum quadro faltante nessa direção."),
    IT("Nessun fotogramma mancante in quella direzione."),
    NL("Geen ontbrekend frame in die richting."),
    RU("В этом направлении нет пропущенных кадров."),
    TR("O yönde eksik kare yok."));

SS_MSG(find_none_scanning,
    EN("No missing frame in that direction yet: the scan is still running."),
    JA("その方向にはまだ欠落フレームがありません。走査はまだ続いています。"),
    ZH_HANS("该方向暂无缺失的帧：扫描仍在进行。"),
    ZH_HANT("該方向暫無缺失的影格：掃描仍在進行。"),
    KO("그 방향에는 아직 누락된 프레임이 없습니다. 검사가 아직 진행 중입니다."),
    DE("In dieser Richtung noch kein fehlendes Bild: Die Prüfung läuft noch."),
    FR("Aucune image manquante dans cette direction pour l'instant : l'analyse est toujours en cours."),
    ES("Aún no hay fotograma faltante en esa dirección: el examen sigue en curso."),
    PT("Nenhum quadro faltante nessa direção ainda: a verificação ainda está em andamento."),
    IT("Ancora nessun fotogramma mancante in quella direzione: la scansione è ancora in corso."),
    NL("Nog geen ontbrekend frame in die richting: het scannen loopt nog."),
    RU("В этом направлении пока нет пропущенных кадров: проверка ещё идёт."),
    TR("O yönde henüz eksik kare yok: tarama hâlâ sürüyor."));

// ===========================================================================
// Slideshow
// ===========================================================================

SS_MSG(slide_play,
    EN("Play"),
    JA("再生"),
    ZH_HANS("播放"),
    ZH_HANT("播放"),
    KO("재생"),
    DE("Abspielen"),
    FR("Lecture"),
    ES("Reproducir"),
    PT("Reproduzir"),
    IT("Riproduci"),
    NL("Afspelen"),
    RU("Воспроизвести"),
    TR("Oynat"));

SS_MSG(slide_stop,
    EN("Stop"),
    JA("停止"),
    ZH_HANS("停止"),
    ZH_HANT("停止"),
    KO("정지"),
    DE("Stopp"),
    FR("Arrêt"),
    ES("Detener"),
    PT("Parar"),
    IT("Ferma"),
    NL("Stoppen"),
    RU("Стоп"),
    TR("Durdur"));

SS_MSG(slide_fps,
    EN("Frame rate"),
    JA("フレームレート"),
    ZH_HANS("帧率"),
    ZH_HANT("影格率"),
    KO("프레임 속도"),
    DE("Bildrate"),
    FR("Cadence"),
    ES("Cadencia"),
    PT("Cadência"),
    IT("Frequenza"),
    NL("Framesnelheid"),
    RU("Частота кадров"),
    TR("Kare hızı"));

SS_MSG(slide_help,
    EN("Plays the corrected masks over their photos at the chosen rate, so a wrong frame is seen at speed. Any key or click stops on the frame shown. Frames are decoded a few ahead; a rate the decoder cannot keep up with shows as a lower shown rate, never as skipped frames."),
    JA("修正済みマスクを写真に重ねて指定レートで再生し、誤ったフレームを素早く見つけます。キーやクリックで表示中のフレームで停止します。数フレーム先までデコードします。デコードが追いつかないレートでは表示レートが下がりますが、フレームは飛ばされません。"),
    ZH_HANS("以选定速率在照片上播放修正后的蒙版，以便快速发现错误的帧。任意按键或点击会停在当前显示的帧。提前解码几帧；解码跟不上的速率表现为显示速率下降，而不会跳帧。"),
    ZH_HANT("以選定速率在照片上播放修正後的遮罩，以便快速發現錯誤的影格。任意按鍵或點擊會停在目前顯示的影格。提前解碼幾格；解碼跟不上的速率表現為顯示速率下降，而不會跳格。"),
    KO("수정된 마스크를 사진 위에 선택한 속도로 재생해 잘못된 프레임을 빠르게 찾습니다. 아무 키나 클릭으로 표시 중인 프레임에서 멈춥니다. 몇 프레임 앞까지 미리 디코딩합니다. 디코더가 따라가지 못하는 속도에서는 표시 속도가 낮아질 뿐 프레임을 건너뛰지 않습니다."),
    DE("Spielt die korrigierten Masken über ihren Fotos mit der gewählten Rate ab, damit ein falsches Bild im Lauf auffällt. Jede Taste und jeder Klick hält auf dem gezeigten Bild an. Einige Bilder werden vorab dekodiert; eine Rate, der der Dekoder nicht folgen kann, zeigt sich als niedrigere Anzeigerate, nie als übersprungene Bilder."),
    FR("Lit les masques corrigés sur leurs photos à la cadence choisie, pour repérer une image fausse au passage. Toute touche ou clic arrête sur l'image affichée. Quelques images sont décodées d'avance ; une cadence que le décodeur ne suit pas se traduit par une cadence affichée plus basse, jamais par des images sautées."),
    ES("Reproduce las máscaras corregidas sobre sus fotos a la cadencia elegida, para ver un fotograma erróneo al vuelo. Cualquier tecla o clic se detiene en el fotograma mostrado. Se decodifican unos fotogramas por adelantado; una cadencia que el decodificador no alcanza se ve como una cadencia mostrada menor, nunca como fotogramas saltados."),
    PT("Reproduz as máscaras corrigidas sobre as suas fotos à cadência escolhida, para ver um quadro errado em andamento. Qualquer tecla ou clique para no quadro mostrado. Alguns quadros são decodificados com antecedência; uma cadência que o decodificador não acompanha aparece como uma cadência mostrada mais baixa, nunca como quadros saltados."),
    IT("Riproduce le maschere corrette sulle loro foto alla frequenza scelta, per notare un fotogramma sbagliato al volo. Qualsiasi tasto o clic ferma sul fotogramma mostrato. Alcuni fotogrammi vengono decodificati in anticipo; una frequenza che il decodificatore non regge si vede come una frequenza mostrata più bassa, mai come fotogrammi saltati."),
    NL("Speelt de gecorrigeerde maskers over hun foto's af op de gekozen snelheid, zodat een fout frame op snelheid opvalt. Elke toets of klik stopt op het getoonde frame. Enkele frames worden vooruit gedecodeerd; een snelheid die de decoder niet bijhoudt, blijkt uit een lagere getoonde snelheid, nooit uit overgeslagen frames."),
    RU("Показывает исправленные маски поверх фото с выбранной частотой, чтобы неверный кадр был заметен на ходу. Любая клавиша или щелчок останавливает на показанном кадре. Несколько кадров декодируются заранее; частота, за которой декодер не успевает, проявляется как меньшая показанная частота, но кадры не пропускаются."),
    TR("Düzeltilmiş maskeleri fotoğraflarının üzerinde seçilen hızda oynatır; yanlış bir kare akışta fark edilir. Herhangi bir tuş veya tıklama gösterilen karede durdurur. Birkaç kare önceden çözülür; çözücünün yetişemediği hız, atlanan kare olarak değil, daha düşük gösterilen hız olarak görünür."));

SS_MSG(slide_stats,
    EN("Slideshow: shown {0} fps, longest gap {1} ms"),
    JA("スライドショー: 表示 {0} fps、最長間隔 {1} ms"),
    ZH_HANS("幻灯片：显示 {0} fps，最长间隔 {1} ms"),
    ZH_HANT("幻燈片：顯示 {0} fps，最長間隔 {1} ms"),
    KO("슬라이드쇼: 표시 {0} fps, 최장 간격 {1} ms"),
    DE("Diaschau: gezeigt {0} fps, längste Lücke {1} ms"),
    FR("Diaporama : affiché {0} fps, plus long écart {1} ms"),
    ES("Presentación: mostrado {0} fps, mayor hueco {1} ms"),
    PT("Apresentação: mostrado {0} fps, maior intervalo {1} ms"),
    IT("Presentazione: mostrati {0} fps, pausa più lunga {1} ms"),
    NL("Diavoorstelling: getoond {0} fps, langste gat {1} ms"),
    RU("Слайд-шоу: показано {0} fps, наибольший разрыв {1} мс"),
    TR("Slayt gösterisi: gösterilen {0} fps, en uzun boşluk {1} ms"));

SS_MSG(slide_waiting,
    EN("Decoding the next frame..."),
    JA("次のフレームをデコード中…"),
    ZH_HANS("正在解码下一帧…"),
    ZH_HANT("正在解碼下一影格…"),
    KO("다음 프레임 디코딩 중…"),
    DE("Nächstes Bild wird dekodiert …"),
    FR("Décodage de l'image suivante…"),
    ES("Decodificando el siguiente fotograma…"),
    PT("Decodificando o próximo quadro…"),
    IT("Decodifica del fotogramma successivo…"),
    NL("Volgende frame wordt gedecodeerd…"),
    RU("Декодирование следующего кадра…"),
    TR("Sonraki kare çözülüyor…"));

// ===========================================================================
// Keys
// ===========================================================================

SS_MSG(hint_keys,
    EN("Left/Right step a frame, PageUp/PageDown ten, Home/End the ends. M/Shift+M next/previous missing. V the view."),
    JA("左右キーで 1 フレーム、PageUp/PageDown で 10 フレーム、Home/End で両端へ移動。M/Shift+M で次/前の欠落へ。V で表示切替。"),
    ZH_HANS("左/右键移动一帧，PageUp/PageDown 移动十帧，Home/End 到两端。M/Shift+M 下一个/上一个缺失。V 切换视图。"),
    ZH_HANT("左/右鍵移動一格，PageUp/PageDown 移動十格，Home/End 到兩端。M/Shift+M 下一個/上一個缺失。V 切換檢視。"),
    KO("왼쪽/오른쪽으로 한 프레임, PageUp/PageDown으로 열 프레임, Home/End로 양 끝. M/Shift+M 다음/이전 누락. V 보기 전환."),
    DE("Links/Rechts ein Bild, PageUp/PageDown zehn, Home/End die Enden. M/Shift+M nächstes/vorheriges fehlendes. V die Ansicht."),
    FR("Gauche/Droite une image, PageUp/PageDown dix, Home/End les extrémités. M/Shift+M manquante suivante/précédente. V la vue."),
    ES("Izquierda/Derecha un fotograma, PageUp/PageDown diez, Home/End los extremos. M/Shift+M faltante siguiente/anterior. V la vista."),
    PT("Esquerda/Direita um quadro, PageUp/PageDown dez, Home/End os extremos. M/Shift+M próximo/anterior faltante. V a vista."),
    IT("Sinistra/Destra un fotogramma, PageUp/PageDown dieci, Home/End gli estremi. M/Shift+M mancante successivo/precedente. V la vista."),
    NL("Links/Rechts één frame, PageUp/PageDown tien, Home/End de uiteinden. M/Shift+M volgende/vorige ontbrekende. V de weergave."),
    RU("Влево/Вправо один кадр, PageUp/PageDown десять, Home/End края. M/Shift+M следующий/предыдущий пропуск. V вид."),
    TR("Sol/Sağ bir kare, PageUp/PageDown on, Home/End uçlar. M/Shift+M sonraki/önceki eksik. V görünüm."));

}  // namespace maskedit
}  // namespace msg
}  // namespace i18n
}  // namespace spirula

#include "i18n/EndCatalog.h"
