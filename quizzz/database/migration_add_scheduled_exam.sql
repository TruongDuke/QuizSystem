-- Migration script: Add scheduled exam support to existing database
-- Run this if you already have a database with data

USE quizDB;

-- Add new columns to Quizzes table
ALTER TABLE Quizzes 
ADD COLUMN exam_type ENUM('normal', 'scheduled') DEFAULT 'normal' AFTER status,
ADD COLUMN exam_start_time DATETIME NULL AFTER exam_type,
ADD COLUMN exam_end_time DATETIME NULL AFTER exam_start_time;

-- Update existing quizzes to be 'normal' type (if not already set)
UPDATE Quizzes SET exam_type = 'normal' WHERE exam_type IS NULL;

