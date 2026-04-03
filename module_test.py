import nanodb

db=nanodb.NanoVaultDB()
db.init()
# print(db.execute('USE test2'))
# print(db.execute('''CREATE TABLE tp (
#           id INT PRIMARY KEY AUTO_INCREMENT,
#           roll_no VARCHAR(10) NOT NULL UNIQUE
#       );'''))
# print(db.execute('''INSERT INTO tp (roll_no)
#       VALUES ("Hey");'''))
# print(db.execute('''INSERT INTO tp (roll_no)
#       VALUES ("Hello");'''))
# print(db.execute('''UPDATE tp SET roll_no="WH" WHERE roll_no="Hey";'''))
# print(db.execute('''DELETE FROM tp WHERE roll_no="Hello";'''))
# print(db.execute('''INSERT INTO tp (roll_no)
#       VALUES ("Woho");'''))
# print(db.execute('''SELECT * FROM tp;'''))
# print(db.execute('''STATISTICS COUNT FROM tp ON roll_no WHERE roll_no="Woho";'''))

print(db.execute('ADD INDICATOR "sma" ("10") ON SYMBOL 1 COLUMN_NO  1 ticks 100;'))
print(db.execute('ENABLE STRATEGY "again" ("10") on symbol 1 column_no 1 ticks 100;'))
db.enter_shell()    