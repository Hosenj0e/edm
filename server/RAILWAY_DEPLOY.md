# 🚂 Деплой на Railway.app - Пошаговая инструкция

## Шаг 1: Регистрация на Railway

1. Перейдите на https://railway.app
2. Нажмите "Start a New Project"
3. Войдите через GitHub

---

## Шаг 2: Создание нового проекта

### ⚠️ ВАЖНО: Мы деплоим только API сервер, НЕ Qt приложение!

**Qt Desktop приложение** работает локально на компьютерах пользователей.  
**Node.js API сервер** (папка `server/`) деплоится на Railway для синхронизации.

### Вариант А: Деплой из GitHub (рекомендуется)

1. **Push код на GitHub**
   ```bash
   cd c:\Users\Artur\offline-edm
   git add server/ railway.toml
   git commit -m "Добавлен Node.js backend API для синхронизации"
   git push origin main
   ```

2. **Создать проект в Railway**
   - Нажмите "+ New Project"
   - Выберите "Deploy from GitHub repo"
   - Выберите репозиторий `edm`
   - Railway начнет сборку
   
3. **Настроить Root Directory (ВАЖНО!)**
   
   Так как в репозитории есть Qt проект в корне, нужно указать Railway использовать только папку `server/`:
   
   - Откройте созданный сервис
   - Перейдите в **Settings** → **General**
   - Найдите **Root Directory**
   - Укажите: `server`
   - Нажмите **Save**
   
   Railway автоматически пересоберет проект, теперь он найдет `package.json` и установит Node.js!
   
   **Альтернативный способ (через railway.json):**
   
   Создайте файл `railway.json` в корне репозитория:
   ```json
   {
     "build": {
       "builder": "NIXPACKS"
     },
     "deploy": {
       "startCommand": "npm start"
     }
   }
   ```
   
   И установите Root Directory в `server` через Settings.

### Вариант Б: Локальный деплой

```bash
cd server
npm install -g @railway/cli
railway login
railway init
railway up
```

---

## Шаг 3: Добавление PostgreSQL

1. В созданном проекте нажмите **"+ New"**
2. Выберите **"Database"**
3. Выберите **"Add PostgreSQL"**
4. Railway автоматически:
   - Создаст базу данных
   - Добавит переменную окружения `DATABASE_URL`
   - Свяжет её с вашим сервисом

---

## Шаг 4: Настройка переменных окружения

1. Откройте ваш сервис (не базу данных)
2. Перейдите на вкладку **"Variables"**
3. Нажмите **"+ New Variable"**

Добавьте следующие переменные:

```
JWT_SECRET=<сгенерируйте случайную строку - см. ниже>
JWT_EXPIRES_IN=7d
NODE_ENV=production
ALLOWED_ORIGINS=*
```

### Генерация JWT_SECRET

**Windows PowerShell:**
```powershell
[Convert]::ToBase64String((1..64 | ForEach-Object { Get-Random -Maximum 256 }))
```

**Linux/Mac:**
```bash
openssl rand -base64 64
```

**Online:**
https://www.random.org/strings/

---

## Шаг 5: Запуск миграций

### Вариант А: Через Railway CLI

```bash
cd server
railway run npm run migrate
```

### Вариант Б: Через Railway Dashboard

1. Откройте проект в Railway
2. Перейдите на вкладку **"Deployments"**
3. Найдите последний деплой (зелёный статус)
4. Нажмите на три точки **"..."** → **"Run Command"**
5. Введите команду:
   ```
   npm run migrate
   ```
6. Нажмите "Run"

Вы увидите вывод:
```
🚀 Запуск миграций базы данных...
✅ Миграции выполнены успешно!
✅ Администратор по умолчанию создан
```

---

## Шаг 6: Получение публичного URL

1. Откройте ваш сервис в Railway
2. Перейдите на вкладку **"Settings"**
3. Найдите секцию **"Domains"**
4. Нажмите **"Generate Domain"**

Railway создаст домен вида:
```
https://edm-sync-server-production.up.railway.app
```

---

## Шаг 7: Проверка работы

### Тест 1: Health Check

```bash
curl https://your-app.railway.app/health
```

Ожидаемый ответ:
```json
{
  "status": "ok",
  "timestamp": "2025-01-15T12:00:00.000Z",
  "database": "connected"
}
```

### Тест 2: Вход

```bash
curl -X POST https://your-app.railway.app/api/auth/login \
  -H "Content-Type: application/json" \
  -d "{\"login\":\"admin\",\"password\":\"admin123\"}"
```

Ожидаемый ответ:
```json
{
  "token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...",
  "user": {
    "userId": "...",
    "login": "admin",
    "displayName": "Администратор",
    "role": "admin"
  }
}
```

---

## Шаг 8: Обновление Qt клиента

Теперь нужно настроить Qt приложение для работы с сервером:

### Вариант А: Через настройки в приложении

Добавьте поле "Server URL" в окно настроек приложения:
```
https://your-app.railway.app
```

### Вариант Б: Hardcode (для демо)

В коде Qt создайте файл `src/sync/SyncConfig.h`:

```cpp
#define SERVER_URL "https://your-app.railway.app"
```

---

## 📊 Мониторинг

### Просмотр логов

**Railway Dashboard:**
1. Откройте проект
2. Выберите сервис
3. Вкладка "Deployments"
4. "View Logs"

**CLI:**
```bash
railway logs
```

### Метрики

Railway показывает:
- 📈 CPU usage
- 💾 Memory usage
- 🌐 Network traffic
- 📊 Request count

---

## 🔄 Автоматические деплои

Railway автоматически деплоит при каждом push в GitHub!

```bash
# Внесли изменения в server/server.js
git add server/
git commit -m "Добавлена новая функция"
git push

# Railway автоматически:
# 1. Обнаружит изменения
# 2. Пересоберет приложение
# 3. Задеплоит новую версию
# 4. Переключит трафик
```

---

## 💰 Стоимость

Railway предоставляет **$5 бесплатно каждый месяц**.

Ваше приложение потребует примерно:
- Node.js сервис: ~$3/месяц
- PostgreSQL: ~$2/месяц
- **Итого: ~$5/месяц = БЕСПЛАТНО!**

Если превысите лимит, Railway начнёт списывать с карты.

---

## 🛠 Troubleshooting

### Проблема: "npm: command not found" при сборке

**Причина:** Railway анализирует корень репозитория и видит Qt/C++ проект вместо Node.js.

**Решение:**
1. Откройте сервис в Railway Dashboard
2. Settings → General → **Root Directory**
3. Укажите: `server`
4. Нажмите Save
5. Railway автоматически пересоберет проект

### Проблема: "Build failed"

**Причины:**
- Нет `package.json` в корне `/server`
- Синтаксические ошибки в коде
- Отсутствуют зависимости

**Решение:**
1. Проверьте логи в Railway Dashboard
2. Убедитесь, что `package.json` содержит `"start": "node server.js"`
3. Локально запустите `npm install && npm start`

### Проблема: "Database connection error"

**Решение:**
1. Убедитесь, что PostgreSQL добавлен в проект
2. Проверьте, что переменная `DATABASE_URL` существует
3. Перезапустите сервис: Deployments → Redeploy

### Проблема: "Cannot run migrations"

**Решение:**
1. Убедитесь, что PostgreSQL запущен
2. Проверьте, что `DATABASE_URL` правильный
3. Запустите миграции вручную через CLI:
   ```bash
   railway run npm run migrate
   ```

### Проблема: "Application crashed"

**Решение:**
1. Откройте логи: Deployments → View Logs
2. Найдите строку с ошибкой (обычно красная)
3. Исправьте ошибку в коде
4. Push изменения на GitHub

---

## 📞 Поддержка

**Railway:**
- Документация: https://docs.railway.app
- Discord: https://discord.gg/railway
- Twitter: @Railway

**Проект:**
- GitHub Issues: https://github.com/Hosenj0e/edm/issues

---

## ✅ Чеклист деплоя

- [ ] Зарегистрировались на Railway
- [ ] Создали проект из GitHub
- [ ] Добавили PostgreSQL
- [ ] Настроили переменные окружения (JWT_SECRET и др.)
- [ ] Запустили миграции (npm run migrate)
- [ ] Получили публичный URL
- [ ] Протестировали /health endpoint
- [ ] Протестировали /api/auth/login
- [ ] Обновили Qt клиент с новым SERVER_URL
- [ ] Изменили пароль admin

---

## 🎉 Готово!

Ваш сервер запущен и готов к работе! 

**Следующие шаги:**
1. Сохраните URL сервера
2. Интегрируйте с Qt приложением
3. Протестируйте синхронизацию
4. Измените пароль администратора
