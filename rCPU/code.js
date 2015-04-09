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
       var cpu_info = data;
       $("#counter").text(cpu_info[0]+" ("+cpu_info.length+" CPUs)");
	   cpuDataSet.append(new Date().getTime(), cpu_info[0]);
	});
}

function initChart(cpuId)
{
  var seriesOptions =
  { 
     strokeStyle: 'rgba(0, 255, 0, 1)',
     fillStyle: 'rgba(0, 255, 0, 0.2)',
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

