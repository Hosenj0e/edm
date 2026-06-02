# Архитектура системы Offline EDM

## Содержание
1. [Общая архитектура системы](#общая-архитектура-системы)
2. [Декомпозиция по слоям](#декомпозиция-по-слоям)
3. [Блок-схема создания документа](#блок-схема-создания-документа)
4. [Блок-схема согласования документа](#блок-схема-согласования-документа)
5. [Блок-схема подписания документа](#блок-схема-подписания-документа)
6. [Блок-схема синхронизации](#блок-схема-синхронизации)
7. [Блок-схема аутентификации](#блок-схема-аутентификации)
8. [Диаграмма компонентов](#диаграмма-компонентов)

---

## Общая архитектура системы

```mermaid
graph TB
    subgraph "Presentation Layer (QML)"
        UI[QML UI]
        LoginPage[LoginPage.qml]
        MainPage[Main.qml]
        AppQML[App.qml]
    end
    
    subgraph "Application Layer (C++)"
        AuthCtrl[AuthController]
        AppCtrl[AppController]
        DocModel[DocumentListModel]
    end
    
    subgraph "Business Logic Layer"
        DocService[DocumentService]
    end
    
    subgraph "Security Layer"
        KeyMgr[KeyManager]
        Encryption[EncryptionService]
        Signature[SignatureProvider]
    end
    
    subgraph "Data Access Layer"
        DocRepo[SqliteDocumentRepository]
        FileStorage[EncryptedFileStorage]
        SyncQueue[SqliteSyncQueue]
    end
    
    subgraph "Synchronization Layer"
        SyncEngine[SyncEngine]
        RemoteGW[RemoteSyncGateway]
    end
    
    subgraph "Storage"
        AccountsDB[(accounts.sqlite)]
        DocsDB[(documents.sqlite)]
        SyncDB[(sync.sqlite)]
        Vault[Encrypted Files]
    end
    
    UI --> AuthCtrl
    UI --> AppCtrl
    AppCtrl --> DocModel
    AppCtrl --> DocService
    AuthCtrl --> AccountsDB
    
    DocService --> DocRepo
    DocService --> FileStorage
    DocService --> Signature
    DocService --> SyncQueue
    
    FileStorage --> Encryption
    Encryption --> KeyMgr
    Signature --> KeyMgr
    
    DocRepo --> DocsDB
    FileStorage --> Vault
    SyncQueue --> SyncDB
    
    SyncEngine --> SyncQueue
    SyncEngine --> RemoteGW
    AppCtrl --> SyncEngine
```

---

## Декомпозиция по слоям

### 1. Presentation Layer (Слой представления)
**Технология:** QML + Qt Quick  
**Ответственность:** Отображение UI, обработка пользовательского ввода

| Компонент | Файл | Назначение |
|-----------|------|------------|
| App | `App.qml` | Корневой компонент, управление навигацией |
| LoginPage | `LoginPage.qml` | Страница авторизации |
| Main | `Main.qml` | Главная страница с документами |

### 2. Application Layer (Слой приложения)
**Технология:** C++ Qt  
**Ответственность:** Координация между UI и бизнес-логикой

| Компонент | Файлы | Назначение |
|-----------|-------|------------|
| AuthController | `AuthController.h/cpp` | Управление аутентификацией |
| AppController | `AppController.h/cpp` | Управление сессией, документами |
| DocumentListModel | `DocumentListModel.h/cpp` | Модель данных для QML ListView |

### 3. Business Logic Layer (Слой бизнес-логики)
**Технология:** C++  
**Ответственность:** Реализация бизнес-правил

| Компонент | Файлы | Назначение |
|-----------|-------|------------|
| DocumentService | `DocumentService.h/cpp` | Операции с документами (CRUD, approve, sign) |

### 4. Security Layer (Слой безопасности)
**Технология:** C++ + Qt Cryptography  
**Ответственность:** Шифрование, подписи, управление ключами

| Компонент | Файлы | Назначение |
|-----------|-------|------------|
| KeyManager | `KeyManager.h/cpp` | Управление криптографическими ключами |
| EncryptionService | `EncryptionService.h/cpp` | Шифрование/дешифрование данных (AES-256) |
| SignatureProvider | `SignatureProvider.h/cpp` | Создание цифровых подписей (HMAC-SHA256) |

### 5. Data Access Layer (Слой доступа к данным)
**Технология:** C++ + SQLite  
**Ответственность:** Хранение и извлечение данных

| Компонент | Файлы | Назначение |
|-----------|-------|------------|
| SqliteDocumentRepository | `SqliteDocumentRepository.h/cpp` | Работа с метаданными документов |
| EncryptedFileStorage | `EncryptedFileStorage.h/cpp` | Хранение зашифрованного контента |
| SqliteSyncQueue | `SqliteSyncQueue.h/cpp` | Очередь задач синхронизации |

### 6. Synchronization Layer (Слой синхронизации)
**Технология:** C++ + Qt Network  
**Ответственность:** Синхронизация с удалённым сервером

| Компонент | Файлы | Назначение |
|-----------|-------|------------|
| SyncEngine | `SyncEngine.h/cpp` | Управление процессом синхронизации |
| RemoteSyncGateway | `RemoteSyncGateway.h/cpp` | Взаимодействие с удалённым API |

### 7. Storage (Хранилище)
**Технология:** SQLite + File System  
**Ответственность:** Персистентное хранение данных

| Хранилище | Расположение | Содержимое |
|-----------|--------------|------------|
| accounts.sqlite | `%APPDATA%/offline-edm/offline-edm/` | Учётные записи пользователей |
| documents.sqlite | `%APPDATA%/offline-edm/offline-edm/users/{login}/` | Метаданные документов |
| sync.sqlite | `%APPDATA%/offline-edm/offline-edm/users/{login}/` | Очередь синхронизации |
| vault/ | `%APPDATA%/offline-edm/offline-edm/users/{login}/vault/` | Зашифрованные файлы |

---

## Блок-схема создания документа

```mermaid
flowchart TD
    Start([Пользователь нажимает<br/>'Создать документ']) --> Input[Ввод названия<br/>и содержимого]
    Input --> CallCreate[Вызов AppController.<br/>createDocument]
    CallCreate --> CheckService{DocumentService<br/>существует?}
    CheckService -->|Нет| ErrorService[Ошибка: сервис не инициализирован]
    CheckService -->|Да| GenID[Генерация UUID<br/>для документа]
    
    GenID --> CalcHash[Вычисление SHA-256<br/>хеша контента]
    CalcHash --> Encrypt[Шифрование контента<br/>AES-256-CBC]
    Encrypt --> SaveFile[Сохранение в<br/>vault/{docId}.enc]
    SaveFile --> CheckFile{Файл<br/>сохранён?}
    CheckFile -->|Нет| ErrorFile[Ошибка записи файла]
    CheckFile -->|Да| SaveMeta[Сохранение метаданных<br/>в documents.sqlite]
    
    SaveMeta --> CheckMeta{Метаданные<br/>сохранены?}
    CheckMeta -->|Нет| ErrorMeta[Ошибка записи в БД]
    CheckMeta -->|Да| CreateApproval[Создание записи<br/>в approvals]
    CreateApproval --> AddToQueue[Добавление задачи<br/>в sync_queue]
    AddToQueue --> Reload[Вызов reloadDocuments]
    Reload --> UpdateModel[Обновление<br/>DocumentListModel]
    UpdateModel --> EmitSignal[Emit documentsModelChanged]
    EmitSignal --> UpdateUI[Обновление UI]
    UpdateUI --> End([Документ создан])
    
    ErrorService --> End
    ErrorFile --> End
    ErrorMeta --> End
    
    style Start fill:#e1f5e1
    style End fill:#e1f5e1
    style ErrorService fill:#ffe1e1
    style ErrorFile fill:#ffe1e1
    style ErrorMeta fill:#ffe1e1
```

---

## Блок-схема согласования документа

```mermaid
flowchart TD
    Start([Пользователь нажимает<br/>'Согласовать']) --> GetDocId[Получение docId<br/>из UI]
    GetDocId --> CallApprove[Вызов AppController.<br/>approveDocument]
    CallApprove --> CheckService{DocumentService<br/>существует?}
    CheckService -->|Нет| ErrorService[Ошибка: сервис не инициализирован]
    CheckService -->|Да| CheckStatus{Статус =<br/>OnApproval?}
    
    CheckStatus -->|Нет| ErrorStatus[Ошибка: неверный статус]
    CheckStatus -->|Да| UpdateStatus[Обновление статуса<br/>на Approved]
    UpdateStatus --> UpdateTime[Обновление<br/>updated_at]
    UpdateTime --> SaveApproval[Сохранение записи<br/>в approvals]
    SaveApproval --> CheckSave{Сохранено?}
    CheckSave -->|Нет| ErrorSave[Ошибка записи]
    CheckSave -->|Да| GetVersion[Получение версии<br/>документа]
    
    GetVersion --> AddToQueue[Добавление задачи<br/>APPROVE в sync_queue]
    AddToQueue --> Reload[Вызов reloadDocuments]
    Reload --> UpdateModel[Обновление<br/>DocumentListModel]
    UpdateModel --> EmitSignal[Emit documentsModelChanged]
    EmitSignal --> UpdateUI[Обновление UI:<br/>статус 'Согласован']
    UpdateUI --> End([Документ согласован])
    
    ErrorService --> End
    ErrorStatus --> End
    ErrorSave --> End
    
    style Start fill:#e1f5e1
    style End fill:#e1f5e1
    style ErrorService fill:#ffe1e1
    style ErrorStatus fill:#ffe1e1
    style ErrorSave fill:#ffe1e1
```

---

## Блок-схема подписания документа

```mermaid
flowchart TD
    Start([Пользователь нажимает<br/>'Подписать']) --> GetDocId[Получение docId<br/>из UI]
    GetDocId --> CallSign[Вызов AppController.<br/>signDocument]
    CallSign --> CheckService{DocumentService<br/>существует?}
    CheckService -->|Нет| ErrorService[Ошибка: сервис не инициализирован]
    CheckService -->|Да| CheckStatus{Статус =<br/>Approved?}
    
    CheckStatus -->|Нет| ErrorStatus[Ошибка: документ<br/>не согласован]
    CheckStatus -->|Да| GetHash[Получение content_hash<br/>из БД]
    GetHash --> CheckHash{Hash<br/>существует?}
    CheckHash -->|Нет| ErrorHash[Ошибка: нет хеша]
    CheckHash -->|Да| GenSignature[Генерация подписи<br/>HMAC-SHA256]
    
    GenSignature --> UpdateStatus[Обновление статуса<br/>на Signed]
    UpdateStatus --> SaveSignature[Сохранение подписи<br/>в signatures]
    SaveSignature --> CheckSave{Сохранено?}
    CheckSave -->|Нет| ErrorSave[Ошибка записи]
    CheckSave -->|Да| GetVersion[Получение версии<br/>документа]
    
    GetVersion --> AddToQueue[Добавление задачи<br/>SIGN в sync_queue]
    AddToQueue --> Reload[Вызов reloadDocuments]
    Reload --> UpdateModel[Обновление<br/>DocumentListModel]
    UpdateModel --> EmitSignal[Emit documentsModelChanged]
    EmitSignal --> UpdateUI[Обновление UI:<br/>статус 'Подписан']
    UpdateUI --> End([Документ подписан])
    
    ErrorService --> End
    ErrorStatus --> End
    ErrorHash --> End
    ErrorSave --> End
    
    style Start fill:#e1f5e1
    style End fill:#e1f5e1
    style ErrorService fill:#ffe1e1
    style ErrorStatus fill:#ffe1e1
    style ErrorHash fill:#ffe1e1
    style ErrorSave fill:#ffe1e1
```

---

## Блок-схема синхронизации

```mermaid
flowchart TD
    Start([Запуск приложения /<br/>Восстановление сети]) --> InitEngine[Инициализация<br/>SyncEngine]
    InitEngine --> CheckOnline{Есть<br/>интернет?}
    CheckOnline -->|Нет| WaitOnline[Ожидание<br/>подключения]
    WaitOnline --> CheckOnline
    CheckOnline -->|Да| GetQueue[Получение задач<br/>из sync_queue]
    
    GetQueue --> CheckEmpty{Очередь<br/>пуста?}
    CheckEmpty -->|Да| EndSync([Синхронизация<br/>завершена])
    CheckEmpty -->|Нет| GetTask[Взять первую задачу]
    GetTask --> CheckType{Тип<br/>операции?}
    
    CheckType -->|CREATE| SyncCreate[Отправка документа<br/>на сервер]
    CheckType -->|APPROVE| SyncApprove[Отправка согласования<br/>на сервер]
    CheckType -->|SIGN| SyncSign[Отправка подписи<br/>на сервер]
    
    SyncCreate --> CheckResponse{Ответ<br/>успешен?}
    SyncApprove --> CheckResponse
    SyncSign --> CheckResponse
    
    CheckResponse -->|Нет| CheckRetry{Попыток<br/>< 3?}
    CheckRetry -->|Да| IncrementRetry[Увеличить счётчик<br/>попыток]
    IncrementRetry --> WaitRetry[Ожидание 5 сек]
    WaitRetry --> GetTask
    CheckRetry -->|Нет| MarkFailed[Пометить задачу<br/>как failed]
    MarkFailed --> GetQueue
    
    CheckResponse -->|Да| RemoveTask[Удалить задачу<br/>из очереди]
    RemoveTask --> UpdatePending[Emit pendingSyncTasksChanged]
    UpdatePending --> GetQueue
    
    style Start fill:#e1f5e1
    style EndSync fill:#e1f5e1
    style MarkFailed fill:#ffe1e1
```

---

## Блок-схема аутентификации

```mermaid
flowchart TD
    Start([Запуск приложения]) --> CheckDB{accounts.sqlite<br/>существует?}
    CheckDB -->|Нет| CreateDB[Создание БД<br/>и таблицы users]
    CheckDB -->|Да| CheckAdmin{Пользователь<br/>admin существует?}
    CreateDB --> CreateAdmin[Создание admin<br/>с паролем 123]
    CheckAdmin -->|Нет| CreateAdmin
    CheckAdmin -->|Да| ShowLogin[Показать<br/>LoginPage]
    CreateAdmin --> ShowLogin
    
    ShowLogin --> InputCreds[Ввод логина<br/>и пароля]
    InputCreds --> ClickLogin[Нажатие кнопки<br/>'Войти']
    ClickLogin --> Validate{Поля<br/>заполнены?}
    Validate -->|Нет| ShowError1[Показать ошибку:<br/>'Заполните все поля']
    ShowError1 --> InputCreds
    
    Validate -->|Да| HashPassword[Вычисление хеша<br/>PBKDF2-SHA256]
    HashPassword --> QueryDB[Запрос к БД:<br/>SELECT * WHERE login=?]
    QueryDB --> CheckUser{Пользователь<br/>найден?}
    CheckUser -->|Нет| ShowError2[Показать ошибку:<br/>'Неверный логин']
    ShowError2 --> InputCreds
    
    CheckUser -->|Да| CompareHash{Хеш<br/>совпадает?}
    CompareHash -->|Нет| ShowError3[Показать ошибку:<br/>'Неверный пароль']
    ShowError3 --> InputCreds
    
    CompareHash -->|Да| SaveSession[Сохранение текущего<br/>пользователя]
    SaveSession --> OpenSession[Вызов AppController.<br/>openSession]
    OpenSession --> InitServices[Инициализация сервисов:<br/>KeyManager, DocumentService, etc.]
    InitServices --> LoadDocs[Загрузка документов<br/>пользователя]
    LoadDocs --> ShowMain[Показать<br/>Main.qml]
    ShowMain --> End([Пользователь<br/>авторизован])
    
    style Start fill:#e1f5e1
    style End fill:#e1f5e1
    style ShowError1 fill:#ffe1e1
    style ShowError2 fill:#ffe1e1
    style ShowError3 fill:#ffe1e1
```

---

## Диаграмма компонентов

```mermaid
graph TB
    subgraph "UI Components"
        App[App.qml<br/>Навигация]
        Login[LoginPage.qml<br/>Авторизация]
        Main[Main.qml<br/>Список документов]
    end
    
    subgraph "Controllers"
        AuthCtrl[AuthController<br/>Управление аутентификацией]
        AppCtrl[AppController<br/>Управление сессией]
    end
    
    subgraph "Models"
        DocModel[DocumentListModel<br/>QAbstractListModel]
    end
    
    subgraph "Services"
        DocService[DocumentService<br/>Бизнес-логика документов]
    end
    
    subgraph "Security"
        KeyMgr[KeyManager<br/>Управление ключами]
        Encrypt[EncryptionService<br/>AES-256-CBC]
        Sign[SignatureProvider<br/>HMAC-SHA256]
    end
    
    subgraph "Repositories"
        DocRepo[SqliteDocumentRepository<br/>Метаданные]
        FileStore[EncryptedFileStorage<br/>Контент]
        SyncQueue[SqliteSyncQueue<br/>Очередь]
    end
    
    subgraph "Sync"
        SyncEng[SyncEngine<br/>Управление синхронизацией]
        Gateway[RemoteSyncGateway<br/>API клиент]
    end
    
    subgraph "Databases"
        AccDB[(accounts.sqlite)]
        DocDB[(documents.sqlite)]
        SyncDB[(sync.sqlite)]
        Vault[(vault/)]
    end
    
    App --> Login
    App --> Main
    Login --> AuthCtrl
    Main --> AppCtrl
    AppCtrl --> DocModel
    AppCtrl --> DocService
    AppCtrl --> SyncEng
    
    AuthCtrl --> AccDB
    
    DocService --> DocRepo
    DocService --> FileStore
    DocService --> Sign
    DocService --> SyncQueue
    
    FileStore --> Encrypt
    Encrypt --> KeyMgr
    Sign --> KeyMgr
    
    DocRepo --> DocDB
    FileStore --> Vault
    SyncQueue --> SyncDB
    
    SyncEng --> SyncQueue
    SyncEng --> Gateway
    
    style App fill:#e3f2fd
    style Login fill:#e3f2fd
    style Main fill:#e3f2fd
    style AuthCtrl fill:#fff3e0
    style AppCtrl fill:#fff3e0
    style DocModel fill:#f3e5f5
    style DocService fill:#e8f5e9
    style KeyMgr fill:#fce4ec
    style Encrypt fill:#fce4ec
    style Sign fill:#fce4ec
    style DocRepo fill:#fff9c4
    style FileStore fill:#fff9c4
    style SyncQueue fill:#fff9c4
    style SyncEng fill:#e0f2f1
    style Gateway fill:#e0f2f1
```

---

## Взаимодействие компонентов при создании документа

```mermaid
sequenceDiagram
    participant UI as QML UI
    participant AC as AppController
    participant DS as DocumentService
    participant FS as EncryptedFileStorage
    participant ES as EncryptionService
    participant DR as DocumentRepository
    participant SQ as SyncQueue
    participant DM as DocumentListModel
    
    UI->>AC: createDocument(title, text)
    AC->>DS: createDocument(title, content)
    DS->>DS: Генерация UUID
    DS->>DS: Вычисление SHA-256
    DS->>FS: writeDocumentContent(docId, plaintext)
    FS->>ES: encrypt(plaintext)
    ES-->>FS: ciphertext
    FS->>FS: Запись в vault/{docId}.enc
    FS-->>DS: success
    DS->>DR: createDocument(docId, title, hash, ...)
    DR->>DR: INSERT INTO documents
    DR->>DR: INSERT INTO approvals
    DR-->>DS: success
    DS->>SQ: enqueue(CREATE, docId, payload)
    SQ->>SQ: INSERT INTO sync_queue
    SQ-->>DS: success
    DS-->>AC: docId
    AC->>AC: reloadDocuments()
    AC->>DS: listDocuments()
    DS->>DR: listDocumentSummaries()
    DR-->>DS: QVector<DocumentSummary>
    DS-->>AC: documents
    AC->>DM: setDocuments(documents)
    DM->>DM: beginResetModel()
    DM->>DM: endResetModel()
    AC->>UI: emit documentsModelChanged()
    UI->>UI: Обновление ListView
```

---

## Ключевые технологии и паттерны

### Архитектурные паттерны
- **Layered Architecture** - разделение на слои (Presentation, Application, Business, Data)
- **Repository Pattern** - абстракция доступа к данным
- **Service Layer** - инкапсуляция бизнес-логики
- **Model-View-Controller (MVC)** - разделение UI и логики

### Паттерны проектирования
- **Dependency Injection** - через конструкторы сервисов
- **Observer Pattern** - Qt Signals/Slots для уведомлений
- **Strategy Pattern** - различные провайдеры (Signature, Encryption)
- **Queue Pattern** - очередь синхронизации

### Технологии безопасности
- **AES-256-CBC** - симметричное шифрование контента
- **PBKDF2-SHA256** - хеширование паролей (100,000 итераций)
- **HMAC-SHA256** - цифровые подписи документов
- **Key Derivation** - генерация ключей из мастер-пароля

### Технологии хранения
- **SQLite** - реляционная БД для метаданных
- **File System** - хранение зашифрованных файлов
- **JSON** - сериализация данных синхронизации

---

## Потоки данных

### Поток создания документа
```
Пользователь → QML → AppController → DocumentService → 
→ EncryptedFileStorage → vault/{docId}.enc
→ SqliteDocumentRepository → documents.sqlite
→ SqliteSyncQueue → sync.sqlite
→ DocumentListModel → QML → Пользователь
```

### Поток синхронизации
```
SyncEngine → SqliteSyncQueue → RemoteSyncGateway → 
→ Удалённый сервер (MVP: local JSON)
→ SqliteSyncQueue (удаление задачи)
→ AppController (emit pendingSyncTasksChanged)
→ QML (обновление счётчика)
```

### Поток аутентификации
```
Пользователь → LoginPage → AuthController → 
→ accounts.sqlite (проверка credentials)
→ AppController.openSession()
→ Инициализация сервисов
→ Main.qml
```

---

## Метрики системы

| Метрика | Значение |
|---------|----------|
| Количество слоёв | 7 |
| Количество компонентов | 18 |
| Количество БД | 3 |
| Алгоритмы шифрования | AES-256-CBC |
| Алгоритмы подписи | HMAC-SHA256 |
| Алгоритмы хеширования | PBKDF2-SHA256, SHA-256 |
| Язык программирования | C++20 |
| UI фреймворк | Qt 6 QML |
| Система сборки | CMake 3.22+ |

---

## Примечания по безопасности

1. **Шифрование данных**: Все файлы документов хранятся в зашифрованном виде (AES-256-CBC)
2. **Хеширование паролей**: Пароли хешируются с использованием PBKDF2-SHA256 (100,000 итераций)
3. **Цифровые подписи**: Документы подписываются с использованием HMAC-SHA256
4. **Изоляция данных**: Каждый пользователь имеет отдельную БД и vault
5. **Offline-first**: Система работает без интернета, синхронизация происходит при восстановлении связи

---

## Масштабируемость

### Горизонтальное масштабирование
- Каждый пользователь имеет изолированное хранилище
- Синхронизация через очередь задач
- Возможность добавления нескольких серверов синхронизации

### Вертикальное масштабирование
- SQLite поддерживает до 281 TB данных
- Файловая система ограничена только размером диска
- Очередь синхронизации может содержать миллионы задач

---

*Документ создан: 2026-05-18*  
*Версия: 1.0*
