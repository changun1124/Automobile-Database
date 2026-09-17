# Project 2 Application README

## 실행 환경

본 프로그램은 Windows 환경과 Visual Studio C++ compiler를 기준으로 작성되었습니다.

필요한 프로그램:
- MySQL Server 8.0
- MySQL Workbench
- MySQL Connector/ODBC
- Visual Studio 2022 C++ Build Tools
- MySQL C API header 및 library

데이터베이스 설정:
Database: project2
ODBC DSN: project2_mysql
Host: 127.0.0.1
Port: 3306
User: root

## 데이터베이스 구축 방법

프로그램을 실행하면 소스 코드 내부에서 다음 SQL 파일을 자동으로 실행하여 데이터베이스를 구축합니다. 프로그램이 정상 종료될 때 생성한 데이터베이스가 삭제됩니다.

## ODBC DSN 설정 방법

이 프로그램은 ODBC가 필요합니다. 따라서 MySQL ODBC Driver를 선택하여 새 DSN을 추가해야 합니다.
DSN 이름: project2_mysql
Server: 127.0.0.1 또는 localhost

## 컴파일 방법

vcvars64.bat를 실행한 다음 소스 코드 폴더로 이동해서 다음 명령어로 컴파일하면 됩니다.
cl /EHsc /std:c++17 /I"C:\Program Files\MySQL\MySQL Server 8.0\include" main.cpp /link /LIBPATH:"C:\Program Files\MySQL\MySQL Server 8.0\lib" odbc32.lib libmysql.lib

만약 `cl` 명령어가 인식되지 않으면 다음 명령어를 실행하면 됩니다.
where /r C:\ vcvars64.bat
call "vcvars64.bat_파일의_전체_경로"

## 실행 방법

실행 전에 반드시 libmysql.dll을 찾을 수 있도록 MySQL library 경로를 환경 변수 "PATH"에 추가해야 합니다. 만약 추가가 되지 않는다면 수동으로 src 폴더 내부에 복사해야 합니다.

다음 명령어를 입력하면 프로그램이 실행됩니다.
main.exe

프로그램을 실행하면 MySQL root password를 입력하라는 메시지가 출력됩니다. root password를 바르게 입력하면 프로그램은 먼저 MySQL C API로 서버에 접속하여 `schema.sql`과 `sample_data.sql`을 자동 실행하고, 그 다음 ODBC와 MySQL C API 연결을 생성한 뒤 메뉴를 출력합니다.

========== Automobile Company Query Menu ==========
1. Sales Trends
2. Defective Part Tracking
3. Top Brands by Revenue
4. Top Brands by Unit Sales
5. Seasonal Sales Patterns
6. Dealer Inventory Efficiency
7. Supplier Coverage
0. Exit
Select menu:
0부터 7까지의 숫자를 입력하여 원하는 query를 실행하면 됩니다. 2, 5번 query는 MySQL C API를 사용하고, 1, 3, 4, 6, 7번 query는 ODBC를 사용합니다.

0번을 선택하면 프로그램이 종료되며, 종료 과정에서 자동으로 `project2` 데이터베이스를 삭제합니다.

## Query 설명

### 1. Sales Trends

최근 3년 동안의 판매 추세를 각 브랜드 순서대로 출력합니다.
- 브랜드
- 판매 연도
- 판매 월
- 판매 주차
- 구매자 성별
- 구매자 소득 구간

### 2. Defective Part Tracking

사용자가 Supplier name keyword, Part type keyword, Start date, End date를 순서대로 입력하면, 입력한 기간 내에서 특정 supplier가 공급한 defective part가 실제로 장착된 차량과 해당 차량을 구매한 customer를 조회합니다.

입력 예시:
Supplier name keyword: Getrag
Part type keyword: Transmission
Start date: 2024-01-01
End date: 2025-12-31

현재 데이터베이스 상의 supplies 테이블에는 다음 세 가지의 defective part supply가 존재합니다. 모든 defective part supply에서 공급된 부품은 모두 defective하다고 가정합니다. 이 3개의 supply를 제외한 나머지 supply의 경우 모두 결함이 없다고 가정합니다.
- Getrag Systems가 2021-06-15 07:55:00에 공급한 180개의 transmissions
- Continental Safety가 2023-11-11 15:45:00에 공급한 300개의 Airbag Module
- Getrag Systems가 2024-03-12 10:05:00에 공급한 220개의 transmissions


### 3. Top Brands by Revenue

현재 날짜 기준 직전 연도에서 1년 동안 판매 금액 기준으로 가장 높은 매출을 기록한 상위 2개 브랜드를 조회합니다.

### 4. Top Brands by Unit Sales

현재 날짜 기준 직전 연도에서 1년 동안 판매 대수 기준으로 상위 2개 등수의 브랜드를 조회합니다. `DENSE_RANK()`를 사용하므로 2위 판매 대수에서 동점이 발생하면 해당 브랜드들을 모두 출력합니다. 예를 들어 판매 대수가 3대, 2대, 2대, 1대 순서라면 1위와 공동 2위 브랜드가 모두 출력됩니다.

### 5. Seasonal Sales Patterns

사용자가 body style keyword를 입력하면, 그 body style의 차량이 어느 월에 가장 많이 판매되었는지 조회합니다. 해당 body style의 판매 대수가 가장 큰 월이 여러 개이면 모두 출력합니다.

입력 예시:
Body style keyword: Convertible

### 6. Dealer Inventory Efficiency

차량을 inventory에 평균적으로 가장 오랫동안 보유한 Dealer의 이름과 평균 보관 기간을 조회합니다.

### 7. Supplier Coverage

가장 많은 distinct model에 부품을 공급하는 supplier와 그 supplier가 공급하는 distinct model의 개수를 조회합니다.

## Data 설명
2번, 5번 query에서 supplier name, part type, body style을 입력해야 할 때가 있는데, 다음은 데이터베이스에 저장된 supplier name, part type, body style 목록입니다.

-supplier name:
'Aisin Drivetrain', 'Bosch Mobility', 'Magna Powertrain', 'Denso Electronics', 'ZF Transmissions', 'Continental Safety', 'Brembo Brakes', 'Michelin Tire', 'LG Energy Solution', 'Getrag Systems'

-part type:
'Engine', 'Transmission', 'Brake System', 'Battery Pack', 'Infotainment Unit', 'Airbag Module', 'Tire Set', 'Suspension', 'Steering System', 'Exhaust System', 'HVAC Unit', 'Body Control Module'

-body style:
'Pickup', 'Convertible', 'SUV', 'Sedan', 'Hatchback'