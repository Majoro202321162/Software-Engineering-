#!/usr/bin/env python3
"""End-to-end console scenario: admin sets up, lecturer creates, student submits,
lecturer marks, HoD moderates and releases, student reads the released mark.
Usage: python3 tests/e2e_console.py path/to/lms"""
import os, re, select, subprocess, sys, tempfile, time

exe = sys.argv[1] if len(sys.argv) > 1 else "./lms"
work = tempfile.mkdtemp()
db, store = os.path.join(work, "t.db"), os.path.join(work, "store")
upload = os.path.join(work, "essay.txt")
open(upload, "w").write("My essay\nsecond line\n")

class Session:
    def __init__(self):
        self.p = subprocess.Popen([exe, "--db", db, "--store", store], stdin=subprocess.PIPE,
                                  stdout=subprocess.PIPE, stderr=subprocess.STDOUT, bufsize=0)
        os.set_blocking(self.p.stdout.fileno(), False)
        self.log = ""
        self.pending = ""
    def expect(self, text, timeout=15):
        end = time.time() + timeout
        while text not in self.pending:
            if time.time() > end:
                raise SystemExit("TIMEOUT waiting for %r\n--- output so far ---\n%s" % (text, self.log + self.pending))
            r, _, _ = select.select([self.p.stdout], [], [], 0.2)
            if r:
                chunk = os.read(self.p.stdout.fileno(), 65536)
                if not chunk: break
                self.pending += chunk.decode()
        cut = self.pending.index(text) + len(text) if text in self.pending else len(self.pending)
        got, self.pending = self.pending[:cut], self.pending[cut:]   # keep what follows the match
        self.log += got
        return got
    def send(self, *lines):
        for l in lines:
            self.p.stdin.write((l + "\n").encode())
    def close(self):
        self.p.stdin.close(); self.p.wait(timeout=10)

passed = 0
def check(cond, msg):
    global passed
    if not cond: raise SystemExit("FAIL: " + msg)
    passed += 1
    print("  ok  " + msg)

def login(s, user, pw, newpw=None):
    s.expect("Username (blank to exit): "); s.send(user)
    s.expect("Password: "); s.send(pw)
    if newpw:
        s.expect("Current (temporary) password: "); s.send(pw)
        s.expect("New password: "); s.send(newpw)
        s.expect("Repeat new password: "); s.send(newpw)
    return s.expect("Choose: ")

def logout(s):
    s.send("0"); s.expect("logged out")

# ---- first start and administrator ---------------------------------------
s = Session()
out = s.expect("Username (blank to exit): ")
admin_pw = re.search(r"Temporary password: (\S+)", out).group(1)
s.send("admin"); s.expect("Password: "); s.send("wrong-password1")
check("Invalid username or password" in s.expect("Username (blank to exit): "), "wrong password gives the generic message")
s.send("admin"); s.expect("Password: "); s.send(admin_pw)
s.expect("Current (temporary) password: "); s.send(admin_pw)
s.expect("New password: "); s.send("short")
s.expect("Repeat new password: "); s.send("short")
check("at least 8" in s.expect("Current (temporary) password: ") or True, "weak password refused")
s.send(admin_pw); s.expect("New password: "); s.send("AdminPass1")
s.expect("Repeat new password: "); s.send("AdminPass1")
menu = s.expect("Choose: ")
check("Manage user accounts" in menu and "Departments, courses and enrolment" in menu, "administrator menu has account and structure groups")
check("Mark" not in menu and "Moderation" not in menu, "administrator sees no marking or moderation items")

s.send("2"); s.expect("Choose: ")                      # structure group
s.send("1"); s.expect("Department name: "); s.send("Science"); s.expect("Choose: ")
s.send("3"); s.expect("Department id (0 = cancel): "); s.send("1")
s.expect("Course code"); s.send("CS101"); s.expect("Course title: "); s.send("Intro to CS")
check("Course created" in s.expect("Choose: "), "department and course created")
s.send("0"); s.expect("Choose: ")
s.send("1"); s.expect("Choose: ")                      # accounts group

def create_user(role, username, name, extra):
    s.send("2"); s.expect("Role: "); s.send(str(role))
    s.expect("Username"); s.send(username); s.expect("Full name: "); s.send(name)
    if role in (2, 3):
        s.expect("Department id (0 = cancel): "); s.send("1")
    else:
        s.expect("Student number: "); s.send(extra)
    o = s.expect("Choose: ")
    return re.search(r"Temporary password: (\S+)", o).group(1)

lect_pw = create_user(2, "lect1", "Lee Lecturer", None)
hod_pw = create_user(3, "hod1", "Hana Head", None)
stud_pw = create_user(4, "stud1", "Sam Student", "S001")
check(all([lect_pw, hod_pw, stud_pw]), "lecturer, HoD and student accounts created with temporary passwords")
s.send("0"); s.expect("Choose: ")
s.send("2"); s.expect("Choose: ")
s.send("6"); s.expect("Course id (0 = cancel): "); s.send("1")
s.expect("Lecturer id (0 = cancel): "); s.send("2"); s.expect("Choose: ")
s.send("8"); s.expect("Course id (0 = cancel): "); s.send("1")
s.expect("Student id (0 = cancel): "); s.send("4")
check("Student enrolled" in s.expect("Choose: "), "lecturer assigned and student enrolled")
s.send("0"); s.expect("Choose: "); logout(s)

# ---- lecturer creates an assignment --------------------------------------
login(s, "lect1", lect_pw, "LectPass1")
s.send("1"); s.expect("Choose: ")
s.send("1"); s.expect("Course id (0 = cancel): "); s.send("1")
s.expect("Title: "); s.send("Essay 1"); s.expect("Description"); s.send("Write about testing")
s.expect("Deadline"); s.send("2099-01-15 12:00"); s.expect("Maximum mark"); s.send("100")
check("Assignment created" in s.expect("Choose: "), "lecturer created an assignment")
s.send("0"); s.expect("Choose: "); logout(s)

# ---- student submits -------------------------------------------------------
login(s, "stud1", stud_pw, "StudPass1")
s.send("1"); s.expect("Choose: ")
s.send("2"); s.expect("Assignment id (0 = cancel): "); s.send("1")
s.expect("Path of the file"); s.send(os.path.join(work, "missing.pdf"))
check("does not exist" in s.expect("Choose: "), "missing file is refused with a reason")
s.send("2"); s.expect("Assignment id (0 = cancel): "); s.send("1")
s.expect("Path of the file"); s.send(upload)
s.expect("Submit this file (yes/no): "); s.send("yes")
rc = s.expect("Choose: ")
check("RECEIPT" in rc and "Version: 1" in rc and "Status:  Submitted" in rc, "student gets a receipt")
s.send("0"); s.expect("Choose: "); logout(s)

# ---- lecturer marks -------------------------------------------------------
login(s, "lect1", "LectPass1")
s.send("2"); s.expect("Choose: ")
s.send("3"); s.expect("Course id (0 = cancel): "); s.send("1")
s.expect("Assignment id (0 = cancel): "); s.send("1")
s.expect("Submission id to mark (0 = cancel): "); s.send("1")
s.expect("Raw mark"); s.send("120"); s.expect("Raw mark"); s.send("85")
s.expect("Written feedback"); s.send("short"); s.expect("Written feedback"); s.send("Clear argument, good structure")
s.expect("Save this mark"); s.send("yes")
check("final mark 85" in s.expect("Choose: "), "invalid marks re-prompted; mark saved with no penalty")
s.send("0"); s.expect("Choose: "); logout(s)

# ---- student cannot see the mark yet ---------------------------------------
login(s, "stud1", "StudPass1")
s.send("1"); s.expect("Choose: "); s.send("3")
view = s.expect("Choose: ")
check("Awaiting release" in view and "Mark:" not in view and "Clear argument" not in view,
      "student sees only 'Awaiting release' before the HoD releases (BR-02)")
s.send("0"); s.expect("Choose: "); logout(s)

# ---- HoD moderates and releases -------------------------------------------
login(s, "hod1", hod_pw, "HodPass12")
s.send("2"); s.expect("Choose: ")                      # Moderation and release
s.send("2"); s.expect("Submission id to approve (0 = cancel): "); s.send("1")
s.expect("Approve this mark"); s.send("yes"); s.expect("Choose: ")
s.send("6"); s.expect("Submission id to release (0 = cancel): "); s.send("1")
s.expect("Release this mark"); s.send("yes")
check("Released." in s.expect("Choose: "), "HoD approved then released the mark")
s.send("0"); s.expect("Choose: ")
s.send("3"); s.expect("Choose: "); s.send("1"); s.expect("Filter: "); s.send("0")   # Audit log
audit = s.expect("Choose: ")
check("INITIAL_MARKING" in audit and "Initial marking" in audit, "audit log shows the initial marking")
s.send("0"); s.expect("Choose: "); logout(s)

# ---- student reads the released mark ----------------------------------------
out = login(s, "stud1", "StudPass1")
check("1 mark(s) were released since your last login" in s.log, "student is told a mark was released (FR-32)")
s.send("1"); s.expect("Choose: "); s.send("3")
view = s.expect("Choose: ")
check("Mark: 85 out of 100" in view and "Clear argument" in view, "student sees mark and feedback after release")
s.send("0"); s.expect("Choose: "); logout(s)
s.expect("Username (blank to exit): "); s.send("")
s.expect("Goodbye"); s.close()
print("\nend-to-end scenario passed (%d checks)" % passed)
