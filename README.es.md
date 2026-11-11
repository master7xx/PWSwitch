# PWSwitch

[English](README.md) · [Русский](README.ru.md)

Hecho por inteligencia artificial bajo la supervisión de una inteligencia ebria.

El proyecto se inspira en herramientas como PowerPlanSwitcher. Quería crear mi propia versión: una aplicación nativa sencilla, cómoda para cambiar de plan y con más acceso a sus ajustes. Es un proyecto personal y todavía tiene margen de mejora.

PWSwitch es una pequeña aplicación para la bandeja de Windows x64 que permite elegir planes de energía y editar sus parámetros. Usa las API nativas Win32 y Windows Power, y no necesita una instalación adicional de .NET ni del entorno de ejecución de Visual C++. Se ha probado en Windows 11; la compatibilidad con Windows 10 no se ha verificado por separado.

La interfaz está disponible en español, inglés y ruso. Por defecto utiliza el idioma de interfaz de Windows y recurre al inglés si no es compatible. El idioma se puede cambiar en el menú de la bandeja. Todos los nombres de planes y las descripciones proceden de Windows y se conservan sin cambios. Windows proporciona los botones de los diálogos del sistema, por lo que pueden aparecer en el idioma del sistema.

El menú permite elegir un plan, ajustar los tiempos de apagado de pantalla y suspensión, abrir el editor avanzado, deshacer el último cambio de energía o salir. Los tiempos rápidos se aplican tanto a la corriente como a la batería. El editor muestra ambos valores por separado y puede mostrar los parámetros que Windows oculta normalmente. El inicio automático es opcional, se aplica al usuario actual y también puede desactivarse en la configuración de inicio de Windows.

El menú también incluye «Acerca de», con la versión, la licencia y la dirección del proyecto.

## Descargar y ejecutar

Descarga `PWSwitch-portable.zip` de la [última versión](https://github.com/master7xx/PWSwitch/releases/latest), extrae el archivo en una carpeta y ejecuta `PWSwitch.exe`. El icono puede estar detrás de la flecha de iconos ocultos de Windows. Para cerrar la aplicación, elige «Salir» en el menú de la bandeja.

## Compilar

Instala Visual Studio o Visual Studio Build Tools con la carga de trabajo «Desarrollo para el escritorio con C++», las herramientas C++ para x64 y un SDK de Windows. Clona el repositorio o descarga y extrae el código fuente, abre PowerShell en la carpeta del proyecto y ejecuta:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1
```

El resultado es `build\PWSwitch.exe`. Cierra cualquier copia en ejecución de ese archivo antes de volver a compilar. Consulta las [instrucciones de prueba](docs/TESTING.md) para ver los comandos y sus efectos. La aplicación no incluye funciones de red, telemetría ni actualización automática. El idioma seleccionado se guarda en `HKCU\Software\PWSwitch\Language`; el inicio automático se guarda por separado si se activa. El ejecutable local no está firmado; revisar el código o reproducir una compilación no demuestra la ausencia de todas las vulnerabilidades. [Licencia MIT](LICENSE).
