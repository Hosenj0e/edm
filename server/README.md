# EDM Sync Server

Backend API для синхронизации системы электронного документооборота с PostgreSQL базой данных.

---

## 🚀 Быстрый старт (локально)

### 1. Установка зависимостей

```bash
cd server
npm install
```

### 2. Настройка окружения

Создайте файл `.env`:

```bash
cp .env.example .env
```

Отредактируйте `.env`:

```env
PORT=3000
NODE_ENV=development
DATABASE_URL=postgresql://user:password@localhost:5432/edm_db
JWT_SECRET=your-random-secret-key-here
JWT_EXPIRES_IN=7d
ALLOWED_ORIGINS=http://localhost:*
```

### 3. Запуск PostgreSQL (Docker)

```bash
docker run --name edm-postgres -e POSTGRES_PASSWORD=password -e POSTGRES_DB=edm_db -p 5432:5432 -d postgres:15
```

### 4. Запуск миграций

```bash
npm run migrate
```

### 5. Запуск сервера

```bash
# Режим разработки (с автоперезагрузкой)
npm run dev

# Продакшн
npm start
```

Сервер запустится на http://localhost:3000

---

## 🌐 Деплой на Railway.app

### Шаг 1: Подготовка

1. Создайте аккаунт на [railway.app](https://railway.app)
2. Установите Railway CLI (опционально):

```bash
npm i -g @railway/cli
railway login
```

### Шаг 2: Создание проекта

**Вариант A: Через веб-интерфейс (рекомендуется)**

1. Перейдите на [railway.app/new](https://railway.app/new)
2. Выберите "Deploy from GitHub repo"
3. Выберите ваш репозиторий `edm`
4. Railway автоматически обнаружит Node.js проект

**Вариант B: Через CLI**

```bash
cd server
railway init
railway up
```

### Шаг 3: Добавление PostgreSQL

1. В проекте Railway нажмите "+ New"
2. Выберите "Database" → "PostgreSQL"
3. Railway автоматически создаст переменную `DATABASE_URL`

### Шаг 4: Настройка переменных окружения

В Railway Dashboard → Variables добавьте:

```
JWT_SECRET=<сгенерируйте случайную строку>
JWT_EXPIRES_IN=7d
NODE_ENV=production
ALLOWED_ORIGINS=*
```

Для генерации JWT_SECRET:

```bash
node -e "console.log(require('crypto').randomBytes(64).toString('hex'))"
```

### Шаг 5: Запуск миграций

В Railway Dashboard:

1. Откройте вкладку "Deployments"
2. Найдите последний деплой
3. Нажмите "View Logs"
4. В меню выберите "Run Command"
5. Введите: `npm run migrate`

Или через CLI:

```bash
railway run npm run migrate
```

### Шаг 6: Получение URL

Railway автоматически создаст публичный URL вида:
```
https://edm-sync-server-production.up.railway.app
```

Найдите его в Settings → Domains

---

## 📡 API Endpoints

### Аутентификация

```http
POST /api/auth/register
Content-Type: application/json

{
  "login": "teacher1",
  "password": "password123",
  "displayName": "Иванов Иван",
  "role": "teacher"
}
```

```http
POST /api/auth/login
Content-Type: application/json

{
  "login": "admin",
  "password": "admin123"
}

Response:
{
  "token": "eyJhbGciOiJIUzI1NiIs...",
  "user": {
    "userId": "...",
    "login": "admin",
    "displayName": "Администратор",
    "role": "admin"
  }
}
```

### Группы

```http
GET /api/groups
Authorization: Bearer <token>

Response: [
  {
    "group_id": "uuid",
    "number": "2991",
    "course": "4",
    "specialty": "Информатика",
    "student_count": 25,
    "sheet_count": 5
  }
]
```

```http
POST /api/groups
Authorization: Bearer <token>
Content-Type: application/json

{
  "number": "2991",
  "course": "4",
  "specialty": "Программная инженерия",
  "institute": "ИЭиУ"
}
```

### Студенты

```http
GET /api/students/<group_id>
Authorization: Bearer <token>

Response: [
  {
    "student_id": "uuid",
    "group_id": "uuid",
    "student_number": "1",
    "fio": "Иванов Иван Иванович",
    "status": "обучается",
    "grade": ""
  }
]
```

```http
POST /api/students
Authorization: Bearer <token>
Content-Type: application/json

{
  "groupId": "uuid",
  "studentNumber": "1",
  "fio": "Петров Петр Петрович",
  "status": "обучается"
}
```

### Ведомости

```http
GET /api/grade-sheets/<group_id>
Authorization: Bearer <token>

Response: [
  {
    "sheet_id": "uuid",
    "title": "Экзамен: Математика - 2991",
    "exam_type": "exam",
    "status": "draft",
    "discipline_name": "Математика"
  }
]
```

```http
PUT /api/grade-sheets/<sheet_id>/approve
Authorization: Bearer <token>

Response: {
  "sheet_id": "uuid",
  "status": "approved",
  "approved_by": "teacher1"
}
```

---

## 🧪 Тестирование API

### С помощью curl

```bash
# Вход
curl -X POST https://your-server.railway.app/api/auth/login \
  -H "Content-Type: application/json" \
  -d '{"login":"admin","password":"admin123"}'

# Сохраните токен
TOKEN="eyJhbGciOiJIUzI1NiIs..."

# Получить группы
curl https://your-server.railway.app/api/groups \
  -H "Authorization: Bearer $TOKEN"
```

### С помощью Postman

1. Импортируйте коллекцию из `postman_collection.json` (если есть)
2. Создайте environment с переменной `base_url` и `token`
3. Выполните Login запрос, токен автоматически сохранится

---

## 🔒 Безопасность

### Рекомендации для продакшн:

1. **Используйте сильный JWT_SECRET**
   ```bash
   node -e "console.log(require('crypto').randomBytes(64).toString('hex'))"
   ```

2. **Настройте CORS правильно**
   ```env
   ALLOWED_ORIGINS=https://your-domain.com,https://app.your-domain.com
   ```

3. **Включите rate limiting** (в будущем)
   ```bash
   npm install express-rate-limit
   ```

4. **Мониторинг и логирование**
   - Railway автоматически собирает логи
   - Добавьте Sentry для отслеживания ошибок

5. **Регулярные бэкапы БД**
   - Railway делает автоматические бэкапы
   - Можно настроить дополнительные через Dashboard

---

## 📊 Мониторинг

### Проверка здоровья сервера

```bash
curl https://your-server.railway.app/health
```

Response:
```json
{
  "status": "ok",
  "timestamp": "2024-01-15T10:30:00.000Z",
  "database": "connected"
}
```

### Просмотр логов Railway

```bash
railway logs
```

Или в веб-интерфейсе: Project → Deployments → View Logs

---

## 🐛 Troubleshooting

### Проблема: "Database connection failed"

**Решение:**
1. Проверьте, что PostgreSQL добавлен в Railway проект
2. Убедитесь, что `DATABASE_URL` установлен в переменных окружения
3. Проверьте логи: `railway logs`

### Проблема: "JWT malformed"

**Решение:**
1. Убедитесь, что `JWT_SECRET` установлен
2. Проверьте формат токена в заголовке: `Authorization: Bearer <token>`

### Проблема: "Port already in use"

**Решение:**
```bash
# Найти процесс на порту 3000
lsof -i :3000  # Mac/Linux
netstat -ano | findstr :3000  # Windows

# Убить процесс
kill -9 <PID>  # Mac/Linux
taskkill /PID <PID> /F  # Windows
```

---

## 📝 Changelog

### v1.0.0 (2025-01-15)
- ✅ Базовая аутентификация (JWT)
- ✅ CRUD для групп, студентов, ведомостей
- ✅ PostgreSQL интеграция
- ✅ Деплой на Railway

---

## 📧 Поддержка

По вопросам работы API:
- GitHub Issues: https://github.com/Hosenj0e/edm/issues
- Email: artur.pezhemskiy@novsu.ru

---

## 📄 Лицензия

MIT License - НовГУ им. Ярослава Мудрого
