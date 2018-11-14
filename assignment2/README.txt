University of Victoria
CSC 360 Fall 2018
Italo Borrelli
V00884840

acs represents an airline check-in with 4 desks allowing economy and business
customers with business customers having first priority to get serviced

Usage is ./ACS <customer_file>

The customer_file must be in the following format:

1. The first character specifies the unique ID of customers.
2. A colon(:) immediately follows the unique number of the customer.
3. Immediately following is an integer equal to either 1 (indicating the customer belongs to business class) or 0
(indicating the customer belongs to economy class).
4. A comma(,) immediately follows the previous number.
5. Immediately following is an integer that indicates the arrival time of the customer.
6. A comma(,) immediately follows the previous number.
7. Immediately following is an integer that indicates the service time of the customer.
8. A newline (\n) ends a line.
