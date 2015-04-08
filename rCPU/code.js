var cpuDataSet = new TimeSeries();

$(function() {
  $("#status").text("");

  get_cpu_use();
  initChart(1);

  setInterval("get_cpu_use()", 500);
});

function get_cpu_use()
{
	$.ajax({
		url: "cpu.api",
		type: "post",
		data: { counter:"0" }
	}).done(function(data)
	{
		$("#counter").text(JSON.parse(data)[0]);
	   cpuDataSet.append(new Date().getTime(), JSON.parse(data)[0]);
	});
}

function initChart(cpuId)
{
  var seriesOptions =
  { 
     strokeStyle: 'rgba(255, 0, 0, 1)',
     fillStyle: 'rgba(255, 0, 0, 0.1)',
     lineWidth: 3 
  };
  var timeline = new SmoothieChart(
  { 
     maxValue: 100,
     minValue: 0,
     millisPerPixel: 20,
     grid:
     {
        strokeStyle: '#555555',
        lineWidth: 1,
        millisPerLine: 1000,
        verticalSections: 4 
     }
  });
  timeline.addTimeSeries(cpuDataSet, seriesOptions);
  timeline.streamTo(document.getElementById('cpu'+cpuId), 500);
}

