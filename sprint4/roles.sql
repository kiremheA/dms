CREATE ROLE app_admin LOGIN PASSWORD :'admin_password';
CREATE ROLE app_user LOGIN NOSUPERUSER PASSWORD :'app_password';
CREATE ROLE app_analyst LOGIN PASSWORD :'analyst_password';

GRANT ALL ON SCHEMA public TO app_admin;
GRANT ALL ON ALL TABLES IN SCHEMA public TO app_admin;

GRANT SELECT, INSERT, UPDATE ON ALL TABLES IN SCHEMA public TO app_user;

GRANT SELECT ON ALL TABLES IN SCHEMA public TO app_analyst;