const { Pool } = require('pg');
require('dotenv').config();

const pool = new Pool({
    connectionString: process.env.DATABASE_URL,
    ssl: process.env.NODE_ENV === 'production' ? { rejectUnauthorized: false } : false
});

const migrations = `
-- Пользователи
CREATE TABLE IF NOT EXISTS users (
    user_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    login VARCHAR(50) UNIQUE NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    display_name VARCHAR(100) NOT NULL,
    role VARCHAR(20) NOT NULL DEFAULT 'teacher',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Учебные группы
CREATE TABLE IF NOT EXISTS study_groups (
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
CREATE TABLE IF NOT EXISTS students (
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
CREATE TABLE IF NOT EXISTS disciplines (
    discipline_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    name TEXT UNIQUE NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Ведомости
CREATE TABLE IF NOT EXISTS grade_sheets (
    sheet_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    group_id UUID REFERENCES study_groups(group_id) ON DELETE CASCADE,
    discipline_id UUID REFERENCES disciplines(discipline_id),
    title TEXT NOT NULL,
    exam_type VARCHAR(20) DEFAULT 'exam',
    status VARCHAR(30) DEFAULT 'draft',
    approved_by VARCHAR(50),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    version INTEGER DEFAULT 1
);

-- Индексы для производительности
CREATE INDEX IF NOT EXISTS idx_students_group ON students(group_id);
CREATE INDEX IF NOT EXISTS idx_grade_sheets_group ON grade_sheets(group_id);
CREATE INDEX IF NOT EXISTS idx_grade_sheets_status ON grade_sheets(status);
CREATE INDEX IF NOT EXISTS idx_students_updated ON students(updated_at);
CREATE INDEX IF NOT EXISTS idx_groups_updated ON study_groups(updated_at);

-- Добавляем дисциплины по умолчанию
INSERT INTO disciplines (name) VALUES 
    ('Математика'),
    ('Физика'),
    ('Программирование'),
    ('Иностранный язык'),
    ('История'),
    ('Философия')
ON CONFLICT (name) DO NOTHING;
`;

async function runMigrations() {
    console.log('🚀 Запуск миграций базы данных...');
    
    try {
        await pool.query(migrations);
        console.log('✅ Миграции выполнены успешно!');
        
        // Создаем администратора по умолчанию
        const bcrypt = require('bcrypt');
        const passwordHash = await bcrypt.hash('admin123', 10);
        
        await pool.query(
            `INSERT INTO users (login, password_hash, display_name, role) 
             VALUES ($1, $2, $3, $4) 
             ON CONFLICT (login) DO NOTHING`,
            ['admin', passwordHash, 'Администратор', 'admin']
        );
        
        console.log('✅ Администратор по умолчанию создан (login: admin, password: admin123)');
        console.log('⚠️  ВАЖНО: Смените пароль после первого входа!');
        
    } catch (error) {
        console.error('❌ Ошибка при выполнении миграций:', error);
        process.exit(1);
    } finally {
        await pool.end();
    }
}

runMigrations();
