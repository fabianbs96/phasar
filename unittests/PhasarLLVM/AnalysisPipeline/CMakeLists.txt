set(AnalysisPipelineSources
  AnalysisPipelineTest.cpp
)

foreach(TEST_SRC ${AnalysisPipelineSources})
  add_phasar_unittest(${TEST_SRC})
endforeach(TEST_SRC)
