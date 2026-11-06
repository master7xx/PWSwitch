#include "i18n.h"
#include <cwchar>
#include <system_error>
#include <stdexcept>

namespace i18n {
namespace {
Language selected = Language::system, effective = Language::english;
const Entry catalog[] = {
{L"Язык", L"Language", L"Idioma"},
{L"Язык Windows", L"Windows language", L"Idioma de Windows"},
{L"English", L"English", L"English"}, {L"Русский", L"Русский", L"Русский"}, {L"Español", L"Español", L"Español"},
{L"Никогда", L"Never", L"Nunca"},
{L"1 минута", L"1 minute", L"1 minuto"}, {L"5 минут", L"5 minutes", L"5 minutos"},
{L"10 минут", L"10 minutes", L"10 minutos"}, {L"15 минут", L"15 minutes", L"15 minutos"},
{L"30 минут", L"30 minutes", L"30 minutos"}, {L"1 час", L"1 hour", L"1 hora"}, {L"2 часа", L"2 hours", L"2 horas"},
{L" мин.", L" min", L" min"}, {L" сек.", L" sec", L" s"},
{L"Неизвестная схема", L"Unknown power plan", L"Plan desconocido"},
{L" (тип схемы) — ", L" (plan category) — ", L" (tipo de plan) — "},
{L"PWSwitch: не удалось определить активную схему", L"PWSwitch: unable to identify the active plan", L"PWSwitch: no se pudo identificar el plan activo"},
{L"Неизвестная ошибка", L"Unknown error", L"Error desconocido"},
{L"Значения записаны, но применить схему не удалось. Проверьте параметры Windows.\r\n\r\n", L"Values were saved, but the plan could not be applied. Check Windows power settings.\r\n\r\n", L"Los valores se guardaron, pero no se pudo aplicar el plan. Revise la configuración de energía de Windows.\r\n\r\n"},
{L"Время в минутах (0 — никогда):", L"Minutes (0 — never):", L"Minutos (0 — nunca):"},
{L"Применить", L"Apply", L"Aplicar"}, {L"Отмена", L"Cancel", L"Cancelar"},
{L"Введите целое число минут от 0 до 71582788.", L"Enter a whole number of minutes from 0 to 71582788.", L"Introduzca un número entero de minutos entre 0 y 71582788."},
{L"Своё время", L"Custom timeout", L"Tiempo personalizado"}, {L"Своё время…", L"Custom timeout…", L"Tiempo personalizado…"},
{L"Выбранная схема больше недоступна. Обновите список схем.", L"The selected plan is no longer available. Refresh the plan list.", L"El plan seleccionado ya no está disponible. Actualice la lista de planes."},
{L"Не удалось заполнить список значений. Изменение параметра недоступно; попробуйте обновить список.", L"Unable to populate the value list. This setting cannot be edited; try refreshing the list.", L"No se pudo cargar la lista de valores. No se puede editar este ajuste; intente actualizar la lista."},
{L"Текущее: ", L"Current: ", L"Actual: "},
{L"◆ Windows обычно не показывает этот параметр.\r\n", L"◆ Windows normally hides this setting.\r\n", L"◆ Windows normalmente oculta este ajuste.\r\n"},
{L"\r\nДиапазон: ", L"\r\nRange: ", L"\r\nIntervalo: "}, {L", шаг: ", L", step: ", L", incremento: "},
{L"\r\nНе удалось прочитать значения: ", L"\r\nUnable to read values: ", L"\r\nNo se pudieron leer los valores: "},
{L"Есть неприменённые изменения. Отбросить их?", L"There are unapplied changes. Discard them?", L"Hay cambios sin aplicar. ¿Desea descartarlos?"},
{L"Выберите вариант с доступным названием или введите число в указанном диапазоне с указанным шагом.", L"Select a named option or enter a number within the indicated range and step.", L"Seleccione una opción con nombre o introduzca un número dentro del intervalo y con el incremento indicados."},
{L"Значение изменилось вне приложения. Текущее значение: ", L"The value changed outside this application. Current value: ", L"El valor cambió fuera de esta aplicación. Valor actual: "},
{L".\r\nЧерновики сохранены. Проверьте значение и при необходимости нажмите «Применить» ещё раз.", L".\r\nYour drafts were kept. Review the value and click Apply again if needed.", L".\r\nSe conservaron los cambios pendientes. Revise el valor y pulse Aplicar de nuevo si es necesario."},
{L"сеть · ", L"AC · ", L"corriente · "}, {L"батарея · ", L"battery · ", L"batería · "},
{L"Значение записано, но применить схему не удалось. Данные обновлены.\r\n\r\n", L"The value was saved, but the plan could not be applied. Values have been refreshed.\r\n\r\n", L"El valor se guardó, pero no se pudo aplicar el plan. Los datos se han actualizado.\r\n\r\n"},
{L"Схема питания:", L"Power plan:", L"Plan de energía:"}, {L"Обновить", L"Refresh", L"Actualizar"},
{L"Показывать скрытые параметры Windows  (◆ — скрыт Windows)", L"Show hidden Windows settings  (◆ — hidden by Windows)", L"Mostrar ajustes ocultos de Windows  (◆ — oculto por Windows)"},
{L"От сети:", L"Plugged in:", L"Con corriente:"}, {L"От батареи:", L"On battery:", L"Con batería:"},
{L"PWSwitch — расширенные настройки", L"PWSwitch — Advanced settings", L"PWSwitch — Ajustes avanzados"},
{L": недоступно", L": unavailable", L": no disponible"}, {L"сеть ", L"AC ", L"corriente "}, {L", батарея ", L", battery ", L", batería "},
{L"Применяется для сети и батареи", L"Applies to AC and battery power", L"Se aplica con corriente y con batería"},
{L"Схемы питания", L"Power plans", L"Planes de energía"}, {L"Экран", L"Display", L"Pantalla"}, {L"Сон", L"Sleep", L"Suspensión"},
{L"Расширенные настройки…", L"Advanced settings…", L"Ajustes avanzados…"}, {L"Электропитание Windows…", L"Windows Power Options…", L"Opciones de energía de Windows…"},
{L"Автозапуск: не удалось прочитать…", L"Startup: unable to read…", L"Inicio automático: error de lectura…"},
{L"Автозапуск: запись занята…", L"Startup: entry is in use…", L"Inicio automático: entrada ocupada…"},
{L"Обновить путь в записи автозапуска…", L"Update startup entry path…", L"Actualizar ruta de inicio automático…"},
{L"Удалить запись автозапуска с другим путём…", L"Remove startup entry with another path…", L"Eliminar entrada de inicio con otra ruta…"},
{L"Выключить автозапуск", L"Disable startup", L"Desactivar inicio automático"}, {L"Включить автозапуск", L"Enable startup", L"Activar inicio automático"},
{L"Windows может отключить автозапуск в настройках", L"Windows may disable startup in Settings", L"Windows puede desactivar el inicio en Configuración"},
{L"Отменить последнее изменение", L"Undo last change", L"Deshacer último cambio"}, {L"Отменить: ", L"Undo: ", L"Deshacer: "}, {L"Выход", L"Exit", L"Salir"},
{L"схема · ", L"plan · ", L"plan · "},
{L"Активная схема изменилась. Откройте меню ещё раз.", L"The active plan changed. Open the menu again.", L"El plan activo cambió. Abra el menú de nuevo."},
{L"Значения изменились вне приложения. Откройте меню ещё раз.", L"Values changed outside this application. Open the menu again.", L"Los valores cambiaron fuera de esta aplicación. Abra el menú de nuevo."},
{L"экран", L"display", L"pantalla"}, {L"сон", L"sleep", L"suspensión"}, {L", сеть и батарея · ", L", AC and battery · ", L", corriente y batería · "},
{L"Не удалось открыть электропитание Windows (код ", L"Unable to open Windows Power Options (code ", L"No se pudieron abrir las opciones de energía de Windows (código "},
{L"PWSwitch — автозапуск", L"PWSwitch — Startup", L"PWSwitch — Inicio automático"},
{L"Запись PWSwitch занята другой программой или имеет неизвестный формат. Она оставлена без изменений.", L"The PWSwitch entry belongs to another program or has an unknown format. It was left unchanged.", L"La entrada PWSwitch pertenece a otro programa o tiene un formato desconocido. No se ha modificado."},
{L"Запись указывает на другой файл:\r\n", L"The entry points to another file:\r\n", L"La entrada apunta a otro archivo:\r\n"},
{L"\r\n\r\nЗаменить её путём к текущей программе? Это может быть другая копия.", L"\r\n\r\nReplace it with this program's path? It may be another copy.", L"\r\n\r\n¿Sustituirla por la ruta de este programa? Puede ser otra copia."},
{L"\r\n\r\nУдалить эту запись? Это может быть другая копия.", L"\r\n\r\nRemove this entry? It may be another copy.", L"\r\n\r\n¿Eliminar esta entrada? Puede ser otra copia."},
{L"Запись добавлена. Windows может отключить её в настройках автозапуска.", L"The entry was added. Windows may disable it in startup settings.", L"Se añadió la entrada. Windows puede desactivarla en la configuración de inicio."},
{L"Схема или параметр изменились вне приложения. Отмена не выполнена, чтобы сохранить новые настройки.", L"The plan or setting changed outside this application. Undo was skipped to preserve the new settings.", L"El plan o ajuste cambió fuera de esta aplicación. No se deshizo el cambio para conservar la nueva configuración."},
{L"Предыдущие значения восстановлены, но применить схему не удалось. Проверьте параметры Windows.", L"Previous values were restored, but the plan could not be applied. Check Windows power settings.", L"Se restauraron los valores anteriores, pero no se pudo aplicar el plan. Revise la configuración de energía de Windows."},
{L"Не удалось подготовить запуск PWSwitch.", L"Unable to initialize PWSwitch startup.", L"No se pudo preparar el inicio de PWSwitch."},
{L"Не удалось проверить, запущен ли PWSwitch.", L"Unable to check whether PWSwitch is already running.", L"No se pudo comprobar si PWSwitch ya está en ejecución."},
{L"PWSwitch уже запущен, но открыть его окно не удалось.", L"PWSwitch is already running, but its window could not be opened.", L"PWSwitch ya está en ejecución, pero no se pudo abrir su ventana."},
{L"PWSwitch — схемы питания", L"PWSwitch — Power plans", L"PWSwitch — Planes de energía"},
{L"Не удалось создать значок в трее.", L"Unable to create the notification area icon.", L"No se pudo crear el icono del área de notificación."},
{L"Windows не разрешила отслеживать смену схемы. Значок обновится при переключении схемы через приложение.", L"Windows did not allow plan change monitoring. The icon will update when switching plans in this application.", L"Windows no permitió supervisar los cambios de plan. El icono se actualizará al cambiar de plan desde esta aplicación."},
{L"Не удалось включить резервное обновление значка. Если он пропадёт после перезапуска проводника, перезапустите PWSwitch.", L"Unable to enable fallback icon updates. If the icon disappears after Explorer restarts, restart PWSwitch.", L"No se pudo activar la actualización de respaldo del icono. Si desaparece tras reiniciar el Explorador, reinicie PWSwitch."},
{L"Тип: сбережение +", L"Type: saving +", L"Tipo: ahorro +"},
{L"Тип: сбережение", L"Type: saving", L"Tipo: ahorro"},
{L"Тип: компромисс", L"Type: compromise", L"Tipo: compromiso"},
{L"Тип: быстродействие", L"Type: speed", L"Tipo: rapidez"},
{L"Тип: быстродействие +", L"Type: speed +", L"Tipo: rapidez +"},
{L"Тип: неизвестен", L"Type: unknown", L"Tipo: desconocido"},
{L"Схема [", L"Plan [", L"Plan ["},
{L"Некорректный путь к программе.", L"Invalid application path.", L"Ruta de aplicación no válida."},
{L"Путь к программе слишком длинный для автозапуска Windows (более 260 символов вместе с кавычками).", L"The application path is too long for Windows startup (over 260 characters including quotes).", L"La ruta es demasiado larga para el inicio de Windows (más de 260 caracteres incluidas las comillas)."},
{L"Запись автозапуска слишком велика; она оставлена без изменений.", L"The startup entry is too large; it was left unchanged.", L"La entrada de inicio es demasiado grande; no se ha modificado."},
{L"Запись автозапуска меняется. Откройте меню снова.", L"The startup entry is changing. Open the menu again.", L"La entrada de inicio está cambiando. Abra el menú de nuevo."},
{L"Не удалось определить путь к программе.", L"Unable to determine the application path.", L"No se pudo determinar la ruta de la aplicación."},
{L"Запись автозапуска занята другой программой или имеет неизвестный формат.", L"The startup entry belongs to another program or has an unknown format.", L"La entrada de inicio pertenece a otro programa o tiene un formato desconocido."},
{L"Запись автозапуска изменилась. Откройте меню снова.", L"The startup entry changed. Open the menu again.", L"La entrada de inicio cambió. Abra el menú de nuevo."},
{L"Слишком длинный путь к системному каталогу Windows.", L"The Windows system directory path is too long.", L"La ruta del directorio de sistema de Windows es demasiado larga."},
{L"\r\nРедактирование недоступно: Windows не предоставила достоверный шаг диапазона. Текущие значения доступны для просмотра.", L"\r\nEditing is unavailable: Windows did not provide a valid range step. Current values can still be viewed.", L"\r\nEdición no disponible: Windows no proporcionó un incremento válido. Los valores actuales se pueden consultar."},
{L"Индекс ", L"Index ", L"Índice "}, {L" — название недоступно", L" — name unavailable", L" — nombre no disponible"},
{L"\r\nWindows не предоставила доступные значения. Редактирование недоступно.", L"\r\nWindows did not provide available values. Editing is unavailable.", L"\r\nWindows no proporcionó los valores disponibles. No se puede editar."},
{L"\r\nWindows не предоставила все сведения: список может быть неполным.", L"\r\nWindows did not provide all metadata: the list may be incomplete.", L"\r\nWindows no proporcionó todos los datos: la lista puede estar incompleta."},
{L"Ошибка ", L"Error ", L"Error "},
{L"Не удалось завершить изменение или безопасно восстановить значение для сети. Проверьте настройки схемы; автоматическая отмена недоступна.", L"Unable to complete the change or safely restore the AC value. Check the plan settings; automatic undo is unavailable.", L"No se pudo completar el cambio ni restaurar con seguridad el valor con corriente. Revise el plan; no se puede deshacer automáticamente."},
{L"Значения могли записаться, но не удалось подтвердить результат; активная схема могла не примениться. Проверьте настройки схемы; автоматическая отмена недоступна.", L"Values may have been saved, but the result could not be verified; the active plan may not have been applied. Check the plan settings; automatic undo is unavailable.", L"Los valores pueden haberse guardado, pero no se pudo verificar el resultado ni aplicar el plan activo. Revise el plan; no se puede deshacer automáticamente."},
{L"Не удалось полностью отменить изменение или восстановить предыдущее состояние. Проверьте настройки схемы.", L"Unable to fully undo the change or restore the previous state. Check the plan settings.", L"No se pudo deshacer todo el cambio ni restaurar el estado anterior. Revise la configuración del plan."},
{L"Значения могли измениться, но не удалось подтвердить отмену; активная схема могла не примениться. Проверьте настройки схемы; автоматическая отмена недоступна.", L"Values may have changed, but undo could not be verified; the active plan may not have been applied. Check the plan settings; automatic undo is unavailable.", L"Los valores pueden haber cambiado, pero no se pudo verificar la restauración ni aplicar el plan activo. Revise el plan; no se puede deshacer automáticamente."},
{L"Windows не вернула активную схему питания.", L"Windows returned no active power plan.", L"Windows no devolvió ningún plan de energía activo."},
{L"Активная схема питания недоступна.", L"The active power plan is unavailable.", L"El plan de energía activo no está disponible."},
{L"Параметр изменился во время отмены.", L"The setting changed while undoing.", L"El ajuste cambió mientras se deshacía el cambio."},
{L"Значение для сети изменилось во время записи.", L"The AC value changed during writing.", L"El valor con corriente cambió durante la escritura."},
{L"Параметр изменился во время записи.", L"The setting changed during writing.", L"El ajuste cambió durante la escritura."},
{L"Не удалось сохранить язык интерфейса.", L"Unable to save the interface language.", L"No se pudo guardar el idioma de la interfaz."},
{L"Не удалось заполнить список схем. Обновите окно настроек.", L"Unable to populate the plan list. Refresh the settings window.", L"No se pudo cargar la lista de planes. Actualice la ventana de ajustes."}
};
void check(LSTATUS code) { if (code != ERROR_SUCCESS) throw std::system_error(static_cast<int>(code), std::system_category()); }
bool valid(Language l) { return l == Language::system || l == Language::english || l == Language::russian || l == Language::spanish; }
void use(Language l) { selected = l; effective = l == Language::system ? resolve(GetUserDefaultUILanguage()) : l; }
struct Key { HKEY h = nullptr; ~Key() { if (h) RegCloseKey(h); } };
}
Language resolve(LANGID id) {
    switch (PRIMARYLANGID(id)) { case LANG_RUSSIAN: return Language::russian; case LANG_SPANISH: return Language::spanish; default: return Language::english; }
}
Language current() { return effective; }
Language preference() { return selected; }
void set_for_testing(Language language) { if (!valid(language)) throw std::invalid_argument("Invalid language"); use(language); }
void initialize() {
    use(Language::system);
    Key key; LSTATUS code = RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\PWSwitch", 0, KEY_QUERY_VALUE, &key.h);
    if (code == ERROR_FILE_NOT_FOUND) return;
    check(code);
    wchar_t value[16]{}; DWORD type = 0, bytes = sizeof(value);
    code = RegQueryValueExW(key.h, L"Language", nullptr, &type, reinterpret_cast<BYTE*>(value), &bytes);
    if (code == ERROR_FILE_NOT_FOUND || code == ERROR_MORE_DATA) return;
    check(code);
    if (type != REG_SZ || bytes < sizeof(wchar_t) || bytes > sizeof(value) || bytes % sizeof(wchar_t) || value[bytes / sizeof(wchar_t) - 1] != L'\0') return;
    // Reject embedded NULs and unknown values rather than interpreting malformed data.
    if (std::wcslen(value) + 1 != bytes / sizeof(wchar_t)) return;
    if (std::wcscmp(value, L"en") == 0) use(Language::english);
    else if (std::wcscmp(value, L"ru") == 0) use(Language::russian);
    else if (std::wcscmp(value, L"es") == 0) use(Language::spanish);
}
void select(Language language) {
    if (!valid(language)) throw std::invalid_argument("Invalid language");
    Key key;
    if (language == Language::system) {
        LSTATUS code = RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\PWSwitch", 0, KEY_SET_VALUE, &key.h);
        if (code != ERROR_FILE_NOT_FOUND) {
            check(code); code = RegDeleteValueW(key.h, L"Language");
            if (code != ERROR_FILE_NOT_FOUND) check(code);
        }
    } else {
        check(RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\PWSwitch", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key.h, nullptr));
        const wchar_t* value = language == Language::russian ? L"ru" : language == Language::spanish ? L"es" : L"en";
        check(RegSetValueExW(key.h, L"Language", 0, REG_SZ, reinterpret_cast<const BYTE*>(value), 3 * sizeof(wchar_t)));
    }
    use(language);
}
const wchar_t* text(const wchar_t* key) {
    if (!key) return L"";
    if (std::wcscmp(key, L"Windows returned no active scheme") == 0) key = L"Windows не вернула активную схему питания.";
    else if (std::wcscmp(key, L"Active scheme is not available") == 0) key = L"Активная схема питания недоступна.";
    else if (std::wcscmp(key, L"Setting changed while undoing") == 0) key = L"Параметр изменился во время отмены.";
    else if (std::wcscmp(key, L"AC value changed during write") == 0) key = L"Значение для сети изменилось во время записи.";
    else if (std::wcscmp(key, L"Setting changed during write") == 0) key = L"Параметр изменился во время записи.";
    for (const auto& entry : catalog) if (std::wcscmp(entry.key, key) == 0)
        return effective == Language::russian ? entry.key : effective == Language::spanish ? entry.spanish : entry.english;
    return key;
}
std::wstring translate(const std::wstring& key) { return text(key.c_str()); }
const Entry* entries() { return catalog; }
size_t entry_count() { return sizeof(catalog) / sizeof(catalog[0]); }
std::wstring scheme_name(const GUID&, const std::wstring& name) { return name; }
}
