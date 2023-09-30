if __name__ == "__main__":
    path_filein = "mytest.txt"
    path_fileout = "mytest.x"
    with open(path_filein, "r", encoding="utf-8") as fpin:
        with open(path_fileout, "wb") as fpout:
            for line in fpin.readlines():
                fpout.write(
                    int(line.strip(), base=16).to_bytes(length=4, byteorder="little")
                )
