    // the client whose detail settings pick each texture's first loaded mip
    static UClient* __Client;

    void Serialize(FArchive& Ar) override;
    INT DefaultLOD();
