# План реализации серверной синхронизации

## Архитектура решения

```
┌─────────────────┐         HTTP/REST API        ┌─────────────────┐
│   Qt Client     │◄─────────────────────────────►│  Backend Server │
│   (SQLite)      │                               │  (PostgreSQL)   │
└─────────────────┘                               └─────────────────┘
```

---

## 1. Backend API Server (Node.js + Express + PostgreSQL)

### 1.1. Структура проекта сервера

```
edm-server/
├── package.json
├── server.js              # Точка входа
├── config/
│   └── database.js        # Конфигурация PostgreSQL
├── models/
│   ├── User.js
│   ├── Group.js
│   ├── Student.js
│   └── GradeSheet.js
├── routes/
│   ├── auth.js            # Аутентификация
│   ├── groups.js          # Управление группами
│   ├── students.js        # Управление студентами
│   └── gradeSheets.js     # Ведомости
├── middleware/
│   └── auth.js            # JWT проверка
└── migrations/
    └── init.sql           # Создание таблиц
```

### 1.2. PostgreSQL схема базы данных

```sql
-- migrations/init.sql

-- Пользователи
CREATE TABLE users (
    user_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    login VARCHAR(50) UNIQUE NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    display_name VARCHAR(100) NOT NULL,
    role VARCHAR(20) NOT NULL, -- 'admin', 'teacher', 'deputy'
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Учебные группы
CREATE TABLE study_groups (
    group_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    number VARCHAR(10) UNIQUE NOT NULL,
    admission_year VARCHAR(10),
    course VARCHAR(5),
    specialty TEXT,
    profile TEXT,
    institute TEXT,
    study_form VARCHAR(50),
    portal_url TEXT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    version INTEGER DEFAULT 1
);

-- Студенты
CREATE TABLE students (
    student_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    group_id UUID REFERENCES study_groups(group_id) ON DELETE CASCADE,
    student_number VARCHAR(20) NOT NULL,
    fio TEXT NOT NULL,
    status VARCHAR(50) DEFAULT 'обучается',
    grade VARCHAR(10) DEFAULT '',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    version INTEGER DEFAULT 1,
    UNIQUE(group_id, student_number)
);

-- Дисциплины
CREATE TABLE disciplines (
    discipline_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    name TEXT UNIQUE NOT NULL
);

-- Ведомости
CREATE TABLE grade_sheets (
    sheet_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    group_id UUID REFERENCES study_groups(group_id) ON DELETE CASCADE,
    discipline_id UUID REFERENCES disciplines(discipline_id),
    title TEXT NOT NULL,
    exam_type VARCHAR(20) DEFAULT 'exam', -- 'exam' or 'credit'
    status VARCHAR(30) DEFAULT 'draft', -- 'draft', 'pending_approval', 'approved'
    approved_by VARCHAR(50),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    version INTEGER DEFAULT 1
);

-- Индексы для производительности
CREATE INDEX idx_students_group ON students(group_id);
CREATE INDEX idx_grade_sheets_group ON grade_sheets(group_id);
CREATE INDEX idx_grade_sheets_status ON grade_sheets(status);
```

### 1.3. REST API Endpoints

#### Аутентификация
```
POST   /api/auth/register       - Регистрация
POST   /api/auth/login          - Вход (получение JWT токена)
POST   /api/auth/refresh        - Обновление токена
```

#### Группы
```
GET    /api/groups              - Получить все группы
GET    /api/groups/:id          - Получить группу по ID
POST   /api/groups              - Создать группу
PUT    /api/groups/:id          - Обновить группу
DELETE /api/groups/:id          - Удалить группу
GET    /api/groups/sync/:since  - Получить измененные группы с timestamp
```

#### Студенты
```
GET    /api/students/:groupId   - Получить студентов группы
POST   /api/students            - Добавить студента
PUT    /api/students/:id        - Обновить студента
DELETE /api/students/:id        - Удалить студента
```

#### Ведомости
```
GET    /api/grade-sheets/:groupId    - Ведомости группы
POST   /api/grade-sheets             - Создать ведомость
PUT    /api/grade-sheets/:id         - Обновить ведомость
PUT    /api/grade-sheets/:id/approve - Утвердить ведомость
```

---

## 2. Модификация Qt клиента

### 2.1. Новые классы для синхронизации

```cpp
// src/sync/SyncConfig.h
class SyncConfig {
public:
    static QString serverUrl();          // https://your-server.com/api
    static void setServerUrl(const QString& url);
    static QString authToken();
    static void setAuthToken(const QString& token);
};

// src/sync/RestApiClient.h
class RestApiClient : public QObject {
    Q_OBJECT
public:
    // Аутентификация
    QNetworkReply* login(const QString& login, const QString& password);
    
    // Синхронизация групп
    QNetworkReply* syncGroups(qint64 lastSyncTimestamp);
    QNetworkReply* uploadGroup(const StudyGroup& group);
    
    // Синхронизация студентов
    QNetworkReply* syncStudents(const QString& groupId);
    QNetworkReply* uploadStudent(const Student& student);
    
signals:
    void syncCompleted();
    void syncFailed(const QString& error);
};

// src/sync/SyncService.h
class SyncService : public QObject {
    Q_OBJECT
public:
    void startSync();           // Запуск полной синхронизации
    void syncGroup(const QString& groupId);
    
private:
    void uploadLocalChanges();  // Отправка локальных изменений
    void downloadServerChanges(); // Получение изменений с сервера
    void resolveConflicts();    // Разрешение конфликтов
};
```

### 2.2. Добавление полей синхронизации в SQLite

```sql
-- Добавить в каждую таблицу:
ALTER TABLE study_groups ADD COLUMN server_id TEXT;
ALTER TABLE study_groups ADD COLUMN last_sync BIGINT;
ALTER TABLE study_groups ADD COLUMN is_synced INTEGER DEFAULT 0;

ALTER TABLE students ADD COLUMN server_id TEXT;
ALTER TABLE students ADD COLUMN last_sync BIGINT;
ALTER TABLE students ADD COLUMN is_synced INTEGER DEFAULT 0;

-- Таблица для отслеживания конфликтов
CREATE TABLE sync_conflicts (
    conflict_id TEXT PRIMARY KEY,
    entity_type TEXT NOT NULL,  -- 'group', 'student', 'sheet'
    entity_id TEXT NOT NULL,
    local_version INTEGER,
    server_version INTEGER,
    local_data TEXT,           -- JSON
    server_data TEXT,          -- JSON
    created_at INTEGER
);
```

---

## 3. Алгоритм синхронизации

### 3.1. Двусторонняя синхронизация

```
1. Загрузка локальных изменений на сервер
   ├─ Найти все записи с is_synced = 0
   ├─ Отправить POST/PUT на сервер
   └─ Пометить is_synced = 1 при успехе

2. Загрузка изменений с сервера
   ├─ GET /api/groups/sync/:lastSyncTimestamp
   ├─ Сравнить версии (version)
   └─ Применить изменения или создать конфликт

3. Разрешение конфликтов
   ├─ Автоматически: последний по времени побеждает
   └─ Вручную: показать пользователю для выбора
```

### 3.2. Стратегии разрешения конфликтов

**Вариант 1: Server wins (сервер всегда прав)**
```cpp
if (localVersion != serverVersion) {
    // Перезаписываем локальные данные серверными
    updateLocalFromServer(serverData);
}
```

**Вариант 2: Last write wins (последний побеждает)**
```cpp
if (localTimestamp > serverTimestamp) {
    uploadToServer(localData);
} else {
    updateLocalFromServer(serverData);
}
```

**Вариант 3: Manual resolution (ручное разрешение)**
```cpp
if (hasConflict) {
    saveConflictForManualReview();
    showConflictDialog();
}
```

---

## 4. Настройка сервера для хостинга

### Вариант A: Heroku (бесплатный tier)

```bash
# 1. Установка Heroku CLI
# https://devcenter.heroku.com/articles/heroku-cli

# 2. Создание приложения
heroku create edm-sync-server

# 3. Добавление PostgreSQL
heroku addons:create heroku-postgresql:mini

# 4. Деплой
git push heroku main

# 5. Запуск миграций
heroku run node migrate.js
```

### Вариант B: Railway (рекомендуется)

```bash
# 1. Регистрация на railway.app
# 2. Подключение GitHub репозитория
# 3. Railway автоматически:
#    - Обнаружит Node.js проект
#    - Создаст PostgreSQL базу
#    - Настроит переменные окружения
```

### Вариант C: DigitalOcean App Platform

```bash
# 1. Создать droplet Ubuntu
# 2. Установить Node.js и PostgreSQL
# 3. Настроить Nginx как reverse proxy
# 4. Получить SSL сертификат (Let's Encrypt)
```

### Вариант D: Ваш собственный сервер НовГУ

```bash
# Если у НовГУ есть сервер для студентов:
# 1. Запросить доступ к серверу
# 2. Установить Node.js
# 3. Настроить PostgreSQL
# 4. Запустить через PM2 или systemd
```

---

## 5. Переменные окружения (.env)

```env
# Server
PORT=3000
NODE_ENV=production

# PostgreSQL
DATABASE_URL=postgresql://user:password@host:5432/edm_db

# JWT
JWT_SECRET=your-super-secret-key-change-this
JWT_EXPIRES_IN=7d

# CORS
ALLOWED_ORIGINS=http://localhost:*,https://your-domain.com
```

---

## 6. Безопасность

### 6.1. JWT аутентификация

```javascript
// Пример middleware/auth.js
const jwt = require('jsonwebtoken');

function authenticateToken(req, res, next) {
    const token = req.headers['authorization']?.split(' ')[1];
    
    if (!token) {
        return res.status(401).json({ error: 'No token provided' });
    }
    
    jwt.verify(token, process.env.JWT_SECRET, (err, user) => {
        if (err) return res.status(403).json({ error: 'Invalid token' });
        req.user = user;
        next();
    });
}
```

### 6.2. HTTPS обязательно

```javascript
// В production всегда используйте HTTPS
if (process.env.NODE_ENV === 'production') {
    app.use((req, res, next) => {
        if (req.header('x-forwarded-proto') !== 'https') {
            res.redirect(`https://${req.header('host')}${req.url}`);
        } else {
            next();
        }
    });
}
```

---

## 7. Пример минимального server.js

```javascript
const express = require('express');
const cors = require('cors');
const { Pool } = require('pg');
require('dotenv').config();

const app = express();
const pool = new Pool({ connectionString: process.env.DATABASE_URL });

app.use(cors());
app.use(express.json());

// Health check
app.get('/health', (req, res) => {
    res.json({ status: 'ok', timestamp: Date.now() });
});

// Пример: получить все группы
app.get('/api/groups', async (req, res) => {
    try {
        const result = await pool.query(
            'SELECT * FROM study_groups ORDER BY number'
        );
        res.json(result.rows);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

// Пример: создать группу
app.post('/api/groups', async (req, res) => {
    const { number, course, specialty } = req.body;
    try {
        const result = await pool.query(
            'INSERT INTO study_groups (number, course, specialty) VALUES ($1, $2, $3) RETURNING *',
            [number, course, specialty]
        );
        res.status(201).json(result.rows[0]);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

const PORT = process.env.PORT || 3000;
app.listen(PORT, () => {
    console.log(`Server running on port ${PORT}`);
});
```

---

## 8. Пример использования в Qt клиенте

```cpp
// Настройка синхронизации
SyncConfig::setServerUrl("https://your-server.com/api");

// Вход и получение токена
RestApiClient* api = new RestApiClient(this);
QNetworkReply* reply = api->login("teacher1", "password");

connect(reply, &QNetworkReply::finished, [=]() {
    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QString token = doc["token"].toString();
    SyncConfig::setAuthToken(token);
    
    // Теперь можем синхронизироваться
    SyncService* sync = new SyncService(this);
    sync->startSync();
});
```

---

## 9. План внедрения (поэтапно)

### Фаза 1: Backend MVP (1-2 дня)
- ✅ Создать Express сервер
- ✅ Настроить PostgreSQL схему
- ✅ Реализовать базовые CRUD endpoints

### Фаза 2: Деплой (1 день)
- ✅ Выбрать хостинг
- ✅ Задеплоить сервер
- ✅ Настроить PostgreSQL
- ✅ Протестировать через Postman

### Фаза 3: Qt клиент (2-3 дня)
- ✅ Добавить RestApiClient
- ✅ Реализовать SyncService
- ✅ Добавить UI для настроек синхронизации
- ✅ Тестирование

### Фаза 4: Продакшн (1 день)
- ✅ Обработка ошибок
- ✅ Логирование
- ✅ Мониторинг
- ✅ Документация

---

## 10. Рекомендуемые инструменты

### Для разработки:
- **Postman** - тестирование API
- **DBeaver** - управление PostgreSQL
- **PM2** - управление Node.js процессом

### Для мониторинга:
- **Sentry** - отслеживание ошибок
- **LogRocket** - логирование
- **UptimeRobot** - проверка доступности

---

## Следующие шаги

1. **Создать серверный репозиторий** (`edm-server`)
2. **Выбрать хостинг** (рекомендую Railway.app)
3. **Реализовать базовый API**
4. **Протестировать с Postman**
5. **Модифицировать Qt клиент**

Хотите, чтобы я создал полный код сервера? Или помочь с конкретным этапом?
