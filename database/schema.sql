CREATE DATABASE IF NOT EXISTS project2;
USE project2;

DROP TRIGGER IF EXISTS trg_after_sale_insert_status;
DROP TRIGGER IF EXISTS trg_after_sale_insert_inventory;

SET FOREIGN_KEY_CHECKS = 0;

DROP TABLE IF EXISTS Sales_Transaction;
DROP TABLE IF EXISTS Dealer_Inventory;
DROP TABLE IF EXISTS Vehicle_Option;
DROP TABLE IF EXISTS Vehicle_Part;
DROP TABLE IF EXISTS Composes;
DROP TABLE IF EXISTS Part_Supplies;
DROP TABLE IF EXISTS Vehicle;
DROP TABLE IF EXISTS Part;
DROP TABLE IF EXISTS Manufacturing_Plant;
DROP TABLE IF EXISTS Model;
DROP TABLE IF EXISTS Dealer;
DROP TABLE IF EXISTS Customer;
DROP TABLE IF EXISTS Supplier;
DROP TABLE IF EXISTS Brand;

SET FOREIGN_KEY_CHECKS = 1;

CREATE TABLE Brand (
    brand_ID CHAR(10) NOT NULL,
    brand_name VARCHAR(50) NOT NULL,
    PRIMARY KEY (brand_ID),
    UNIQUE (brand_name)
);

CREATE TABLE Model (
    model_ID CHAR(10) NOT NULL,
    model_name VARCHAR(80) NOT NULL,
    model_year YEAR NOT NULL,
    body_style VARCHAR(40) NOT NULL,
    base_price DECIMAL(12,2) NOT NULL,
    brand_ID CHAR(10) NOT NULL,
	PRIMARY KEY (model_ID),

	CONSTRAINT chk_model_base_price
        CHECK (base_price >= 0),

    CONSTRAINT fk_model_brand
        FOREIGN KEY (brand_ID)
        REFERENCES Brand(brand_ID)
        ON UPDATE CASCADE
        ON DELETE RESTRICT
);

CREATE TABLE Supplier (
    supplier_ID CHAR(10) NOT NULL,
    supplier_name VARCHAR(80) NOT NULL,
    contact_information VARCHAR(255) NULL,
    PRIMARY KEY (supplier_ID)
);

CREATE TABLE Manufacturing_Plant (
    plant_ID CHAR(10) NOT NULL,
    plant_name VARCHAR(80) NOT NULL,
    location VARCHAR(120) NOT NULL,
    plant_type VARCHAR(30) NOT NULL,
    supplier_ID CHAR(10) NOT NULL,
    PRIMARY KEY (plant_ID),

	CONSTRAINT chk_plant_type
        CHECK (plant_type IN ('PART_PRODUCTION', 'ASSEMBLY')),

    CONSTRAINT fk_plant_supplier
        FOREIGN KEY (supplier_ID)
        REFERENCES Supplier(supplier_ID)
        ON UPDATE CASCADE
        ON DELETE RESTRICT
);

CREATE TABLE Part (
    part_ID CHAR(10) NOT NULL,
    part_type VARCHAR(50) NOT NULL,
    plant_ID CHAR(10) NOT NULL,
    PRIMARY KEY (part_ID),

    CONSTRAINT fk_part_plant
        FOREIGN KEY (plant_ID)
        REFERENCES Manufacturing_Plant(plant_ID)
        ON UPDATE CASCADE
        ON DELETE RESTRICT
);

CREATE TABLE Part_Supplies (
    supply_ID CHAR(10) NOT NULL,
    supplier_ID CHAR(10) NOT NULL,
    part_ID CHAR(10) NOT NULL,
    supplied_date DATETIME NOT NULL,
    quantity INT NOT NULL,
    is_defective BOOLEAN NOT NULL DEFAULT FALSE,
    PRIMARY KEY (supply_ID),

	CONSTRAINT chk_supply_quantity
        CHECK (quantity > 0),

    CONSTRAINT fk_supply_supplier
        FOREIGN KEY (supplier_ID)
        REFERENCES Supplier(supplier_ID)
        ON UPDATE CASCADE
        ON DELETE RESTRICT,

    CONSTRAINT fk_supply_part
        FOREIGN KEY (part_ID)
        REFERENCES Part(part_ID)
        ON UPDATE CASCADE
        ON DELETE RESTRICT
);

CREATE TABLE Dealer (
    dealer_ID CHAR(10) NOT NULL,
    dealer_name VARCHAR(80) NOT NULL,
    address VARCHAR(255) NULL,
    PRIMARY KEY (dealer_ID)
);

CREATE TABLE Vehicle (
    VIN CHAR(17) NOT NULL,
    manufacturing_date DATETIME NULL,
    current_status VARCHAR(30) NOT NULL,
    model_ID CHAR(10) NOT NULL,
    plant_ID CHAR(10) NOT NULL,
    PRIMARY KEY (VIN),

    CONSTRAINT chk_vehicle_status
        CHECK (current_status IN ('IN_PRODUCTION', 'IN_INVENTORY', 'SOLD')),

    CONSTRAINT fk_vehicle_model
        FOREIGN KEY (model_ID)
        REFERENCES Model(model_ID)
        ON UPDATE CASCADE
        ON DELETE RESTRICT,

    CONSTRAINT fk_vehicle_plant
        FOREIGN KEY (plant_ID)
        REFERENCES Manufacturing_Plant(plant_ID)
        ON UPDATE CASCADE
        ON DELETE RESTRICT
);

CREATE TABLE Vehicle_Option (
    VIN CHAR(17) NOT NULL,
    color VARCHAR(40) NOT NULL,
    engine_type VARCHAR(50) NOT NULL,
    transmission_type VARCHAR(50) NOT NULL,

    PRIMARY KEY (VIN, color, engine_type, transmission_type),

    CONSTRAINT fk_option_vehicle
        FOREIGN KEY (VIN)
        REFERENCES Vehicle(VIN)
        ON UPDATE CASCADE
        ON DELETE CASCADE
);

CREATE TABLE Customer (
    customer_ID CHAR(10) NOT NULL,
    customer_name VARCHAR(80) NOT NULL,
    address VARCHAR(255) NULL,
    phone VARCHAR(20) NULL,
    gender VARCHAR(20) NULL,
    annual_income DECIMAL(12,2) NULL,
    PRIMARY KEY (customer_ID),
    
    CONSTRAINT chk_customer_gender
        CHECK (gender IN ('MALE', 'FEMALE', 'OTHER')),

    CONSTRAINT chk_customer_income
        CHECK (annual_income >= 0)
);

CREATE TABLE Sales_Transaction (
    sale_ID CHAR(10) NOT NULL,
    sale_date DATETIME NOT NULL,
    payment_method VARCHAR(30) NULL,
    sale_price DECIMAL(12,2) NULL,
    dealer_ID CHAR(10) NOT NULL,
    customer_ID CHAR(10) NOT NULL,
    VIN CHAR(17) NOT NULL,
    PRIMARY KEY (sale_ID),
    UNIQUE (VIN),

	CONSTRAINT chk_payment_method
        CHECK (payment_method IN ('CASH', 'CARD', 'LOAN', 'LEASE')),

    CONSTRAINT chk_sale_price
        CHECK (sale_price >= 0),

    CONSTRAINT fk_transaction_dealer
        FOREIGN KEY (dealer_ID)
        REFERENCES Dealer(dealer_ID)
        ON UPDATE CASCADE
        ON DELETE RESTRICT,

    CONSTRAINT fk_transaction_customer
        FOREIGN KEY (customer_ID)
        REFERENCES Customer(customer_ID)
        ON UPDATE CASCADE
        ON DELETE RESTRICT,

    CONSTRAINT fk_transaction_vehicle
        FOREIGN KEY (VIN)
        REFERENCES Vehicle(VIN)
        ON UPDATE CASCADE
        ON DELETE RESTRICT
);

CREATE TABLE Composes (
    model_ID CHAR(10) NOT NULL,
    part_ID CHAR(10) NOT NULL,
    PRIMARY KEY (part_ID, model_ID),

    CONSTRAINT fk_composes_model
        FOREIGN KEY (model_ID)
        REFERENCES Model(model_ID)
        ON UPDATE CASCADE
        ON DELETE CASCADE,

    CONSTRAINT fk_composes_part
        FOREIGN KEY (part_ID)
        REFERENCES Part(part_ID)
        ON UPDATE CASCADE
        ON DELETE CASCADE
);

CREATE TABLE Vehicle_Part (
    VIN CHAR(17) NOT NULL,
    part_ID CHAR(10) NOT NULL,
    supply_ID CHAR(10) NOT NULL,
    PRIMARY KEY (VIN, part_ID),

    CONSTRAINT fk_vehicle_part_vehicle
        FOREIGN KEY (VIN)
        REFERENCES Vehicle(VIN)
        ON UPDATE CASCADE
        ON DELETE CASCADE,

    CONSTRAINT fk_vehicle_part_part
        FOREIGN KEY (part_ID)
        REFERENCES Part(part_ID)
        ON UPDATE CASCADE
        ON DELETE RESTRICT,

    CONSTRAINT fk_vehicle_part_supply
        FOREIGN KEY (supply_ID)
        REFERENCES Part_Supplies(supply_ID)
        ON UPDATE CASCADE
        ON DELETE RESTRICT
);

CREATE TABLE Dealer_Inventory (
    dealer_ID CHAR(10) NOT NULL,
    VIN CHAR(17) NOT NULL,
    inventory_start_date DATETIME NULL,
    inventory_end_date DATETIME NULL,
    PRIMARY KEY (dealer_ID, VIN),
    UNIQUE (VIN),

	CONSTRAINT chk_inventory_dates
        CHECK (inventory_end_date IS NULL OR inventory_end_date >= inventory_start_date),

    CONSTRAINT fk_inventory_dealer
        FOREIGN KEY (dealer_ID)
        REFERENCES Dealer(dealer_ID)
        ON UPDATE CASCADE
        ON DELETE RESTRICT,

    CONSTRAINT fk_inventory_vehicle
        FOREIGN KEY (VIN)
        REFERENCES Vehicle(VIN)
        ON UPDATE CASCADE
        ON DELETE RESTRICT
);

CREATE INDEX idx_model_brand
ON Model(brand_ID);

CREATE INDEX idx_model_body_style
ON Model(body_style);

CREATE INDEX idx_vehicle_model
ON Vehicle(model_ID);

CREATE INDEX idx_vehicle_plant
ON Vehicle(plant_ID);

CREATE INDEX idx_transaction_date
ON Sales_Transaction(sale_date);

CREATE INDEX idx_transaction_dealer
ON Sales_Transaction(dealer_ID);

CREATE INDEX idx_transaction_customer
ON Sales_Transaction(customer_ID);

CREATE INDEX idx_customer_gender_income
ON Customer(gender, annual_income);

CREATE INDEX idx_supply_supplier_part_date
ON Part_Supplies(supplier_ID, part_ID, supplied_date);

CREATE INDEX idx_supply_defective_date
ON Part_Supplies(is_defective, supplied_date);

CREATE INDEX idx_inventory_dealer_dates
ON Dealer_Inventory(dealer_ID, inventory_start_date, inventory_end_date);

CREATE INDEX idx_composes_model
ON Composes(model_ID);

CREATE INDEX idx_composes_part
ON Composes(part_ID);

CREATE INDEX idx_vehicle_part_supply
ON Vehicle_Part(supply_ID);

CREATE INDEX idx_vehicle_part_part
ON Vehicle_Part(part_ID);

DELIMITER //

CREATE TRIGGER trg_after_sale_insert_status
AFTER INSERT ON Sales_Transaction
FOR EACH ROW
BEGIN
    UPDATE Vehicle
    SET current_status = 'SOLD'
    WHERE VIN = NEW.VIN;
END//

CREATE TRIGGER trg_after_sale_insert_inventory
AFTER INSERT ON Sales_Transaction
FOR EACH ROW
BEGIN
    UPDATE Dealer_Inventory
    SET inventory_end_date = NEW.sale_date
    WHERE dealer_ID = NEW.dealer_ID
      AND VIN = NEW.VIN
      AND inventory_end_date IS NULL;
END//

DELIMITER ;
