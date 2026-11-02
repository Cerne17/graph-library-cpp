"""Hand-computed timing fixtures for the independent Azure summary exporter."""
import contextlib
import csv
import hashlib
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

ROOT=Path(__file__).resolve().parent.parent
sys.path.insert(0,str(ROOT/'analysis'))
import validate_traversal_reachability as reachability
import summarize_traversals as summary
import azure_traversals as azure
import prepare_traversals as planning
import estimate_traversals as forecast
import shutil


class TraversalTests(unittest.TestCase):
    def setUp(self):
        temp=tempfile.TemporaryDirectory();self.addCleanup(temp.cleanup)
        self.base=Path(temp.name);self.data=self.base/'raw';self.out=self.base/'out'
        self.config=self.base/'config.csv';self.config.write_text('graph,weighting,representations\ntiny,weighted,csr\n')
        root=self.data/'tiny';root.mkdir(parents=True)
        (root/'input.json').write_text(json.dumps({'vertices':4,'sha256_plaintext':'abc'}))
        for algorithm in ('bfs','dfs'):
            directory=root/f'timing_csr_{algorithm}';directory.mkdir()
            with (directory/'timings.csv').open('w',newline='') as dst:
                writer=csv.writer(dst);writer.writerow(['algorithm','source','seed','elapsed_micros','reachable_vertices','status'])
                writer.writerows((algorithm,source,2025,elapsed,reachable,'ok') for source,elapsed,reachable in [(1,1000000,2),(2,3000000,2),(3,2000000,1)])
            job={'status':'ok','settings':{'graph_sha256':'abc','k':3,'seed':2025,'representation':'csr','algorithm':algorithm,'binary_sha256':'def'},'run':{'hostname':'fixture'},'graph_rss_minus_baseline_bytes':10*1024**2}
            (directory/'job.json').write_text(json.dumps(job));self.seal(directory)

    def seal(self,directory):
        path=directory/'job.json';job=json.loads(path.read_text())
        job['artifacts']={'timings.csv':hashlib.sha256((directory/'timings.csv').read_bytes()).hexdigest()}
        path.write_text(json.dumps(job))

    def summarize(self,expected=3):
        with contextlib.redirect_stdout(io.StringIO()):summary.summarize(self.data,self.config,self.out,expected)

    def test_hand_computed_average_median_stddev_and_units(self):
        self.summarize()
        with (self.out/'results_traversals.csv').open() as src:rows=list(csv.DictReader(src))
        by_metric={r['metric']:r for r in rows}
        self.assertEqual(float(by_metric['bfs_micros']['value_average']),2000000)
        self.assertEqual(float(by_metric['bfs_micros']['value_median']),2000000)
        self.assertEqual(by_metric['bfs_micros']['samples'],'3')
        self.assertEqual(float(by_metric['memory_mb_graph']['value_average']),10)
        with (self.out/'traversal_summary.csv').open() as src:detail=list(csv.DictReader(src))[0]
        self.assertEqual(float(detail['sample_stddev_micros']),1000000)
        self.assertEqual(float(detail['min_micros']),1000000)
        self.assertEqual(float(detail['max_micros']),3000000)

    def test_partial_or_mismatched_comparisons_rejected(self):
        with self.assertRaisesRegex(ValueError,'settings'):self.summarize(expected=100)
        directory=self.data/'tiny/timing_csr_dfs'
        path=directory/'timings.csv';path.write_text(path.read_text().replace('2000000,1,ok','2000000,2,ok'))
        self.seal(directory)
        with self.assertRaisesRegex(ValueError,'reachable'):self.summarize()
        self.assertFalse((self.out/'results_traversals.csv').exists())

    def test_bitmatrix_padding_and_eligibility(self):
        self.assertEqual(planning.bitmatrix_bytes(63), 512)
        self.assertEqual(planning.bitmatrix_bytes(64), 1040)
        self.assertEqual(planning.bitmatrix_bytes(375000), 17580046880)
        self.assertLess(planning.bitmatrix_bytes(375000), 28*1024**3)
        self.assertGreater(planning.bitmatrix_bytes(4843750), 28*1024**3)

    def test_matrix_results_are_separate_and_sources_match_csr(self):
        self.config.write_text(self.config.read_text().replace("weighted,csr", "unweighted,csr|matrix"))
        root = self.data / 'tiny'
        for algorithm in ('bfs', 'dfs'):
            directory = root / f'timing_matrix_{algorithm}'
            shutil.copytree(root / f'timing_csr_{algorithm}', directory)
            path = directory / 'job.json'
            job = json.loads(path.read_text()); job['settings']['representation'] = 'matrix'
            path.write_text(json.dumps(job))
        self.summarize()
        with (self.out/'traversal_summary.csv').open() as src:
            rows = list(csv.DictReader(src))
        self.assertEqual({(r['representation'], r['algorithm']) for r in rows},
                         {('csr','bfs'), ('csr','dfs'), ('matrix','bfs'), ('matrix','dfs')})
        directory = root / 'timing_matrix_bfs'
        path = directory/'timings.csv'
        path.write_text(path.read_text().replace('bfs,1,2025', 'bfs,4,2025'))
        self.seal(directory)
        with self.assertRaisesRegex(ValueError, 'sources'):self.summarize()

    def test_variable_counts_preserve_paired_comparison(self):
        self.config.write_text('graph,weighting,representations,k_csr,k_matrix\ntiny,unweighted,csr|matrix,3,2\n')
        root = self.data/'tiny'
        for algorithm in ('bfs','dfs'):
            directory = root/f'timing_matrix_{algorithm}'
            shutil.copytree(root/f'timing_csr_{algorithm}', directory)
            path = directory/'timings.csv'
            path.write_text('\n'.join(path.read_text().splitlines()[:3])+'\n')
            path = directory/'job.json';job = json.loads(path.read_text())
            job['settings'].update(representation='matrix',k=2)
            path.write_text(json.dumps(job));self.seal(directory)
            path = root/f'timing_csr_{algorithm}/timings.csv'
            path.write_text(path.read_text().replace('2000000,1,ok','8000000,1,ok'))
            self.seal(path.parent)
        self.summarize(expected=None)
        with (self.out/'results_traversals_paired.csv').open() as src:
            paired = list(csv.DictReader(src))
        self.assertTrue(all(r['samples']=='2' for r in paired))
        self.assertTrue(all(float(r['value_average'])==2000000 for r in paired))
        with (self.out/'results_traversals.csv').open() as src:
            full = next(r for r in csv.DictReader(src) if r['representation']=='csr' and r['metric']=='bfs_micros')
        self.assertEqual(full['samples'],'3')
        self.assertEqual(float(full['value_average']),4000000)

    def test_cost_based_rounding_and_component_work(self):
        self.assertEqual(forecast.planned_count(2, budget=300),100)
        self.assertEqual(forecast.planned_count(12, budget=300),20)
        self.assertEqual(forecast.planned_count(100, budget=300),10)
        root = self.base/'stats_fixture';(root/'stats').mkdir(parents=True)
        (root/'stats/summary.csv').write_text('metric,value\nvertices,4\nedges,2\n')
        (root/'stats/components.csv').write_text('component,vertices,root\n1,3,1\n2,1,4\n')
        _, reachable, work, per_source = forecast.component_work(root,{1,4})
        self.assertEqual(reachable,2.5)
        self.assertEqual(work,9.5)
        self.assertEqual(per_source[1],(11,3))
        self.assertEqual(per_source[4],(5,1))

    def test_component_edge_density_is_probability_weighted(self):
        import gzip
        root=self.base/'mixed';(root/'stats').mkdir(parents=True)
        (root/'stats/summary.csv').write_text('metric,value\nvertices,5\nedges,4\n')
        (root/'stats/components.csv').write_text('component,vertices,root\n1,3,1\n2,2,4\n')
        with gzip.open(root/'stats/vertices.csv.gz','wt') as dst:
            dst.write('id,degree,component\n1,2,1\n2,2,1\n3,2,1\n4,1,2\n5,1,2\n')
        _, reachable, work, per_source=forecast.component_work(root,{1,5})
        self.assertAlmostEqual(reachable,2.6)
        self.assertAlmostEqual(work,12)
        self.assertEqual(per_source[1],(14,3))
        self.assertEqual(per_source[5],(9,2))

    def test_reachable_counts_match_independent_component_sizes(self):
        import gzip
        stats=self.base/'statistics/tiny/stats';stats.mkdir(parents=True)
        (stats/'summary.csv').write_text('metric,value\nvertices,4\ncomponent_count,3\n')
        (stats/'components.csv').write_text('component,vertices,root\n1,2,1\n2,1,3\n3,1,4\n')
        with gzip.open(stats/'vertices.csv.gz','wt') as dst:
            dst.write('id,degree,component\n1,1,1\n2,1,1\n3,0,2\n4,0,3\n')
        artifacts={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in stats.iterdir()}
        (stats/'job.json').write_text(json.dumps({'status':'ok','settings':{'graph_sha256':'abc'},'artifacts':artifacts}))
        result=reachability.validate(self.data,self.base/'statistics',self.config)
        self.assertEqual(result['samples_verified'],6)
        path=self.data/'tiny/timing_csr_bfs/timings.csv'
        path.write_text(path.read_text().replace('2000000,1,ok','2000000,2,ok'))
        with self.assertRaisesRegex(ValueError,'component size'):
            reachability.validate(self.data,self.base/'statistics',self.config)

    def test_shutdown_failure_deallocates_vm(self):
        def command(*args,**kwargs):
            if args[:3]==('az','vm','list'):return '[]'
        with mock.patch.object(azure.Path,'is_file',return_value=True), mock.patch.object(azure,'run',side_effect=command), mock.patch('setup_azure_guard.setup',side_effect=RuntimeError('shutdown setup failed')), mock.patch.object(azure.subprocess,'run') as stop:
            with self.assertRaisesRegex(RuntimeError,'shutdown'):azure.provision()
        stop.assert_called_once_with(['az','vm','deallocate','-g','cos242-traversals','-n','cos242-runner','-o','none'],check=False)


if __name__=='__main__':unittest.main()
