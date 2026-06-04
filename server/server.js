const express = require('express');
const cors = require('cors');
const { Pool } = require('pg');
const bcrypt = require('bcrypt');
const jwt = require('jsonwebtoken');
require('dotenv').config();

const app = express();
const PORT = process.env.PORT || 3000;

// PostgreSQL Connection Pool
const pool = new Pool({
    connectionString: process.env.DATABASE_URL,
    ssl: process.env.NODE_ENV === 'production' ? { rejectUnauthorized: false } : false
});

// Middleware
app.use(cors({
    origin: process.env.ALLOWED_ORIGINS?.split(',') || '*'
}));
app.use(express.json());

// Логирование запросов
app.use((req, res, next) => {
    console.log(`${new Date().toISOString()} ${req.method} ${req.path}`);
    next();
});

// Middleware для проверки JWT токена
function authenticateToken(req, res, next) {
    const authHeader = req.headers['authorization'];
    const token = authHeader && authHeader.split(' ')[1];

    if (!token) {
        return res.status(401).json({ error: 'Требуется аутентификация' });
    }

    jwt.verify(token, process.env.JWT_SECRET, (err, user) => {
        if (err) {
            return res.status(403).json({ error: 'Недействительный токен' });
        }
        req.user = user;
        next();
    });
}

// ==================== AUTH ROUTES ====================

// Регистрация
app.post('/api/auth/register', async (req, res) => {
    const { login, password, displayName, role = 'teacher' } = req.body;

    if (!login || !password || !displayName) {
        return res.status(400).json({ error: 'Не все поля заполнены' });
    }

    try {
        const passwordHash = await bcrypt.hash(password, 10);
        const result = await pool.query(
            'INSERT INTO users (login, password_hash, display_name, role) VALUES ($1, $2, $3, $4) RETURNING user_id, login, display_name, role',
            [login, passwordHash, displayName, role]
        );

        res.status(201).json({
            message: 'Пользователь создан',
            user: result.rows[0]
        });
    } catch (error) {
        if (error.code === '23505') {
            res.status(409).json({ error: 'Логин уже существует' });
        } else {
            res.status(500).json({ error: error.message });
        }
    }
});

// Вход
app.post('/api/auth/login', async (req, res) => {
    const { login, password } = req.body;

    if (!login || !password) {
        return res.status(400).json({ error: 'Логин и пароль обязательны' });
    }

    try {
        const result = await pool.query(
            'SELECT * FROM users WHERE login = $1',
            [login]
        );

        if (result.rows.length === 0) {
            return res.status(401).json({ error: 'Неверный логин или пароль' });
        }

        const user = result.rows[0];
        const validPassword = await bcrypt.compare(password, user.password_hash);

        if (!validPassword) {
            return res.status(401).json({ error: 'Неверный логин или пароль' });
        }

        const token = jwt.sign(
            { userId: user.user_id, login: user.login, role: user.role },
            process.env.JWT_SECRET,
            { expiresIn: process.env.JWT_EXPIRES_IN || '7d' }
        );

        res.json({
            token,
            user: {
                userId: user.user_id,
                login: user.login,
                displayName: user.display_name,
                role: user.role
            }
        });
    } catch (error) {
        res.status(500).json({ error: error.message });
    }
});

// ==================== GROUPS ROUTES ====================

// Получить все группы
app.get('/api/groups', authenticateToken, async (req, res) => {
    try {
        const result = await pool.query(
            `SELECT g.*, 
                    (SELECT COUNT(*) FROM students WHERE group_id = g.group_id) as student_count,
                    (SELECT COUNT(*) FROM grade_sheets WHERE group_id = g.group_id) as sheet_count
             FROM study_groups g 
             ORDER BY g.number`
        );
        res.json(result.rows);
    } catch (error) {
        res.status(500).json({ error: error.message });
    }
});

// Получить группу по ID
app.get('/api/groups/:id', authenticateToken, async (req, res) => {
    try {
        const result = await pool.query(
            'SELECT * FROM study_groups WHERE group_id = $1',
            [req.params.id]
        );
        
        if (result.rows.length === 0) {
            return res.status(404).json({ error: 'Группа не найдена' });
        }
        
        res.json(result.rows[0]);
    } catch (error) {
        res.status(500).json({ error: error.message });
    }
});

// Создать группу
app.post('/api/groups', authenticateToken, async (req, res) => {
    const { number, admissionYear, course, specialty, profile, institute, studyForm, portalUrl } = req.body;

    if (!number) {
        return res.status(400).json({ error: 'Номер группы обязателен' });
    }

    try {
        const result = await pool.query(
            `INSERT INTO study_groups (number, admission_year, course, specialty, profile, institute, study_form, portal_url)
             VALUES ($1, $2, $3, $4, $5, $6, $7, $8)
             RETURNING *`,
            [number, admissionYear, course, specialty, profile, institute, studyForm, portalUrl]
        );

        res.status(201).json(result.rows[0]);
    } catch (error) {
        if (error.code === '23505') {
            res.status(409).json({ error: 'Группа с таким номером уже существует' });
        } else {
            res.status(500).json({ error: error.message });
        }
    }
});

// Обновить группу
app.put('/api/groups/:id', authenticateToken, async (req, res) => {
    const { number, admissionYear, course, specialty, profile, institute, studyForm, portalUrl } = req.body;

    try {
        const result = await pool.query(
            `UPDATE study_groups 
             SET number = COALESCE($1, number),
                 admission_year = COALESCE($2, admission_year),
                 course = COALESCE($3, course),
                 specialty = COALESCE($4, specialty),
                 profile = COALESCE($5, profile),
                 institute = COALESCE($6, institute),
                 study_form = COALESCE($7, study_form),
                 portal_url = COALESCE($8, portal_url),
                 updated_at = CURRENT_TIMESTAMP,
                 version = version + 1
             WHERE group_id = $9
             RETURNING *`,
            [number, admissionYear, course, specialty, profile, institute, studyForm, portalUrl, req.params.id]
        );

        if (result.rows.length === 0) {
            return res.status(404).json({ error: 'Группа не найдена' });
        }

        res.json(result.rows[0]);
    } catch (error) {
        res.status(500).json({ error: error.message });
    }
});

// Удалить группу
app.delete('/api/groups/:id', authenticateToken, async (req, res) => {
    try {
        const result = await pool.query(
            'DELETE FROM study_groups WHERE group_id = $1 RETURNING group_id',
            [req.params.id]
        );

        if (result.rows.length === 0) {
            return res.status(404).json({ error: 'Группа не найдена' });
        }

        res.json({ message: 'Группа удалена', groupId: result.rows[0].group_id });
    } catch (error) {
        res.status(500).json({ error: error.message });
    }
});

// Синхронизация: получить измененные группы с определенного времени
app.get('/api/groups/sync/:since', authenticateToken, async (req, res) => {
    const since = new Date(parseInt(req.params.since));
    
    try {
        const result = await pool.query(
            'SELECT * FROM study_groups WHERE updated_at > $1 ORDER BY updated_at',
            [since]
        );
        res.json(result.rows);
    } catch (error) {
        res.status(500).json({ error: error.message });
    }
});

// ==================== STUDENTS ROUTES ====================

// Получить студентов группы
app.get('/api/students/:groupId', authenticateToken, async (req, res) => {
    try {
        const result = await pool.query(
            'SELECT * FROM students WHERE group_id = $1 ORDER BY student_number',
            [req.params.groupId]
        );
        res.json(result.rows);
    } catch (error) {
        res.status(500).json({ error: error.message });
    }
});

// Добавить студента
app.post('/api/students', authenticateToken, async (req, res) => {
    const { groupId, studentNumber, fio, status = 'обучается', grade = '' } = req.body;

    if (!groupId || !studentNumber || !fio) {
        return res.status(400).json({ error: 'Обязательные поля: groupId, studentNumber, fio' });
    }

    try {
        const result = await pool.query(
            `INSERT INTO students (group_id, student_number, fio, status, grade)
             VALUES ($1, $2, $3, $4, $5)
             RETURNING *`,
            [groupId, studentNumber, fio, status, grade]
        );

        res.status(201).json(result.rows[0]);
    } catch (error) {
        if (error.code === '23505') {
            res.status(409).json({ error: 'Студент с таким номером уже существует в группе' });
        } else {
            res.status(500).json({ error: error.message });
        }
    }
});

// Обновить студента
app.put('/api/students/:id', authenticateToken, async (req, res) => {
    const { fio, status, grade } = req.body;

    try {
        const result = await pool.query(
            `UPDATE students 
             SET fio = COALESCE($1, fio),
                 status = COALESCE($2, status),
                 grade = COALESCE($3, grade),
                 updated_at = CURRENT_TIMESTAMP,
                 version = version + 1
             WHERE student_id = $4
             RETURNING *`,
            [fio, status, grade, req.params.id]
        );

        if (result.rows.length === 0) {
            return res.status(404).json({ error: 'Студент не найден' });
        }

        res.json(result.rows[0]);
    } catch (error) {
        res.status(500).json({ error: error.message });
    }
});

// Удалить студента
app.delete('/api/students/:id', authenticateToken, async (req, res) => {
    try {
        const result = await pool.query(
            'DELETE FROM students WHERE student_id = $1 RETURNING student_id',
            [req.params.id]
        );

        if (result.rows.length === 0) {
            return res.status(404).json({ error: 'Студент не найден' });
        }

        res.json({ message: 'Студент удален', studentId: result.rows[0].student_id });
    } catch (error) {
        res.status(500).json({ error: error.message });
    }
});

// ==================== GRADE SHEETS ROUTES ====================

// Получить ведомости группы
app.get('/api/grade-sheets/:groupId', authenticateToken, async (req, res) => {
    try {
        const result = await pool.query(
            `SELECT gs.*, d.name as discipline_name, g.number as group_number
             FROM grade_sheets gs
             JOIN disciplines d ON d.discipline_id = gs.discipline_id
             JOIN study_groups g ON g.group_id = gs.group_id
             WHERE gs.group_id = $1
             ORDER BY gs.created_at DESC`,
            [req.params.groupId]
        );
        res.json(result.rows);
    } catch (error) {
        res.status(500).json({ error: error.message });
    }
});

// Создать ведомость
app.post('/api/grade-sheets', authenticateToken, async (req, res) => {
    const { groupId, disciplineId, title, examType = 'exam' } = req.body;

    if (!groupId || !disciplineId || !title) {
        return res.status(400).json({ error: 'Обязательные поля: groupId, disciplineId, title' });
    }

    try {
        const result = await pool.query(
            `INSERT INTO grade_sheets (group_id, discipline_id, title, exam_type)
             VALUES ($1, $2, $3, $4)
             RETURNING *`,
            [groupId, disciplineId, title, examType]
        );

        res.status(201).json(result.rows[0]);
    } catch (error) {
        res.status(500).json({ error: error.message });
    }
});

// Утвердить ведомость
app.put('/api/grade-sheets/:id/approve', authenticateToken, async (req, res) => {
    try {
        const result = await pool.query(
            `UPDATE grade_sheets 
             SET status = 'approved',
                 approved_by = $1,
                 updated_at = CURRENT_TIMESTAMP,
                 version = version + 1
             WHERE sheet_id = $2
             RETURNING *`,
            [req.user.login, req.params.id]
        );

        if (result.rows.length === 0) {
            return res.status(404).json({ error: 'Ведомость не найдена' });
        }

        res.json(result.rows[0]);
    } catch (error) {
        res.status(500).json({ error: error.message });
    }
});

// ==================== DISCIPLINES ROUTES ====================

// Получить все дисциплины
app.get('/api/disciplines', authenticateToken, async (req, res) => {
    try {
        const result = await pool.query('SELECT * FROM disciplines ORDER BY name');
        res.json(result.rows);
    } catch (error) {
        res.status(500).json({ error: error.message });
    }
});

// Добавить дисциплину
app.post('/api/disciplines', authenticateToken, async (req, res) => {
    const { name } = req.body;

    if (!name) {
        return res.status(400).json({ error: 'Название дисциплины обязательно' });
    }

    try {
        const result = await pool.query(
            'INSERT INTO disciplines (name) VALUES ($1) RETURNING *',
            [name]
        );
        res.status(201).json(result.rows[0]);
    } catch (error) {
        if (error.code === '23505') {
            res.status(409).json({ error: 'Дисциплина уже существует' });
        } else {
            res.status(500).json({ error: error.message });
        }
    }
});

// ==================== HEALTH CHECK ====================

app.get('/health', async (req, res) => {
    try {
        await pool.query('SELECT 1');
        res.json({ 
            status: 'ok', 
            timestamp: new Date().toISOString(),
            database: 'connected'
        });
    } catch (error) {
        res.status(503).json({ 
            status: 'error', 
            timestamp: new Date().toISOString(),
            database: 'disconnected',
            error: error.message
        });
    }
});

app.get('/', (req, res) => {
    res.json({
        name: 'EDM Sync API',
        version: '1.0.0',
        endpoints: {
            auth: '/api/auth/login, /api/auth/register',
            groups: '/api/groups',
            students: '/api/students/:groupId',
            gradeSheets: '/api/grade-sheets/:groupId',
            disciplines: '/api/disciplines',
            health: '/health'
        }
    });
});

// Запуск сервера
app.listen(PORT, () => {
    console.log(`🚀 Server is running on port ${PORT}`);
    console.log(`📍 Environment: ${process.env.NODE_ENV || 'development'}`);
    console.log(`🗄️  Database: ${process.env.DATABASE_URL ? 'Connected' : 'Not configured'}`);
});

// Graceful shutdown
process.on('SIGTERM', async () => {
    console.log('SIGTERM received, closing server...');
    await pool.end();
    process.exit(0);
});
