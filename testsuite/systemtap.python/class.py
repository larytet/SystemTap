import sys

class Account:
    def __init__(self, name, balance):
        self.name = name
        self.balance = balance
    def deposit(self, amt):
        self.balance = self.balance + amt
    def withdrawal(self, amt):
        self.balance = self.balance - amt
    def inquiry(self):
        return self.balance

if __name__ == "__main__":
    acct1 = Account("checking", 0)
    acct2 = Account("savings", 30)
    acct1.deposit(15)
    acct2.withdrawal(2)
    acct1.withdrawal(5)
    acct2.deposit(5)
    print ("Checkings current balance is: %d" % acct1.inquiry())
    print ("Savings current balance is: %d" % acct2.inquiry())

    sys.exit(0)
