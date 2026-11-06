# PWSwitch

PWSwitch es una pequeña aplicación para la bandeja de Windows x64 que permite elegir planes de energía y editar sus parámetros. Usa las API nativas Win32 y Windows Power, y no necesita una instalación adicional de .NET ni del entorno de ejecución de Visual C++. Se ha probado en Windows 11; la compatibilidad con Windows 10 no se ha verificado por separado.

La interfaz está disponible en español, inglés y ruso. Por defecto utiliza el idioma de interfaz de Windows y recurre al inglés si no es compatible. El idioma se puede cambiar en el menú de la bandeja. Todos los nombres de planes y las descripciones proceden de Windows y se conservan sin cambios. Windows proporciona los botones de los diálogos del sistema, por lo que pueden aparecer en el idioma del sistema.

El menú permite elegir un plan, ajustar los tiempos de apagado de pantalla y suspensión, abrir el editor avanzado, deshacer el último cambio de energía o salir. Los tiempos rápidos se aplican tanto a la corriente como a la batería. El editor muestra ambos valores por separado y puede mostrar los parámetros que Windows oculta normalmente. El inicio automático es opcional, se aplica al usuario actual y también puede desactivarse en la configuración de inicio de Windows.

Para compilar desde la raíz del repositorio, instala las herramientas C++ para x64 y el SDK de Windows y ejecuta:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1
```

El resultado es `build\PWSwitch.exe`. Consulta [README.md](README.md) para ver todas las pruebas. La aplicación no incluye funciones de red, telemetría ni actualización automática. El idioma seleccionado se guarda en `HKCU\Software\PWSwitch\Language`; el inicio automático se guarda por separado si se activa. El ejecutable local no está firmado; revisar el código o reproducir una compilación no demuestra la ausencia de todas las vulnerabilidades. [Licencia MIT](LICENSE).
