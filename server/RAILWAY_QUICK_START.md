# Railway Quick Start - Быстрый деплой за 5 минут

## ⚡ Краткая инструкция

### 1. Создать проект (1 мин)

1. Зайти на https://railway.app
2. "New Project" → "Deploy from GitHub repo"
3. Выбрать репозиторий `edm`

### 2. Настроить Root Directory (1 мин)

**ВАЖНО:** Так как в корне репозитория Qt проект, указываем Railway использовать `server/`:

1. Открыть созданный сервис
2. **Settings** → **General**
3. **Root Directory**: `server`
4. **Save**

Railway пересоберет проект и найдет Node.js!

### 3. Добавить PostgreSQL (1 мин)

1. Нажать **"+ New"** в проекте
2. **"Database"** → **"PostgreSQL"**
3. Railway автоматически создаст `DATABASE_URL`

### 4. Переменные окружения (1 мин)

В сервисе (не БД!): **Variables** → **+ New Variable**

```env
JWT_SECRET=<сгенерируйте случайную строку>
NODE_ENV=production
```

**Генерация JWT_SECRET (PowerShell):**
```powershell
-join ((65..90) + (97..122) + (48..57) | Get-Random -Count 64 | ForEach-Object {[char]$_})
```

### 5. Запустить миграции (1 мин)

**Вариант A: Railway CLI**
```bash
npm install -g @railway/cli
railway login
railway run npm run migrate
```

**Вариант B: Railway Dashboard**
1. **Deployments** → последний деплой
2. **"..."** → **"Run Command"**
3. Команда: `npm run migrate`

### 6. Получить URL (1 мин)

1. **Settings** → **Networking**
2. **Public Networking** → **Generate Domain**

Готово! URL будет вида:
```
https://edm-production-XXXX.up.railway.app
```

---

## ✅ Проверка

### Test 1: Health Check
```bash
curl https://ваш-url.railway.app/health
```

Ожидается:
```json
{
  "status": "ok",
  "database": "connected"
}
```

### Test 2: Login
```bash
curl -X POST https://ваш-url.railway.app/api/auth/login \
  -H "Content-Type: application/json" \
  -d '{"login":"admin","password":"admin123"}'
```

Ожидается:
```json
{
  "token": "eyJ...",
  "user": { ... }
}
```

---

## 🔧 Если что-то пошло не так

### Ошибка: "npm: command not found"

**Причина:** Root Directory не установлен в `server`

**Решение:**
```
Settings → General → Root Directory: server → Save
```

### Ошибка: "Database connection failed"

**Причина:** PostgreSQL не добавлен или `DATABASE_URL` отсутствует

**Решение:**
1. Добавить PostgreSQL: "+ New" → "Database" → "PostgreSQL"
2. Проверить Variables → должен быть `DATABASE_URL`

### Ошибка: "Migrations failed"

**Причина:** БД недоступна или миграции уже выполнены

**Решение:**
1. Проверить логи: Deployments → View Logs
2. Попробовать снова: `railway run npm run migrate`

---

## 📱 Сохраните URL

После успешного деплоя сохраните URL сервера, он понадобится для Qt клиента:

```cpp
// В Qt приложении
const QString SERVER_URL = "https://ваш-url.railway.app";
```

---

## 💡 Полезные команды Railway CLI

```bash
# Логи в реальном времени
railway logs

# Запуск команды в контейнере
railway run <команда>

# Просмотр переменных
railway variables

# Открыть проект в браузере
railway open
```

---

## 📊 Мониторинг

**Railway Dashboard показывает:**
- 📈 CPU/Memory usage
- 🌐 Request count
- 📉 Response times
- 📋 Real-time logs

**Лимит бесплатного тарифа:**
- $5 кредитов/месяц
- ~550 часов работы
- Ваш сервер: ~$2-3/месяц

---

## 🎉 Готово!

Ваш backend API запущен и готов к работе!

**Следующий шаг:** Интегрировать с Qt клиентом для синхронизации данных.
